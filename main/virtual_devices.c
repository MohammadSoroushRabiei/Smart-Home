#include "virtual_devices.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "mqtt_manager.h"
#include "time_sync.h"
#include "app_state.h"
#include "lcd_driver.h"
#include "ui_screens.h"

static const char *TAG = "virtual_dev";

#define VD_TASK_PERIOD_MS   5000
#define VD_TASK_STACK       3072
#define VD_TASK_PRIORITY    2

// ثابت‌های محیط - باید با ml/generate_data.py یکی بماند
#define VD_SUNRISE_H        6.4f
#define VD_SUNSET_H         18.6f
#define VD_HOME_FORCE_MS    (45ULL * 60ULL * 1000ULL)   // پس از باز شدن قفل

static SemaphoreHandle_t s_mutex = NULL;
static bool s_presence = true;
static float s_lux = 2.0f;
static bool s_values_valid = false;

// پنجره‌ی «کاربر قطعاً خانه» بعد از باز شدن قفل
static int64_t s_home_until_us = 0;

// کش برنامه‌ی روز جاری (با یک LCG seeded از تاریخ روز - مثل Python)
static int s_cached_yday = -1;
static bool s_cached_weekend;
static float s_away[4];        // حداکثر ۲ بازه‌ی غیبت: [شروع۱، پایان۱، شروع۲، پایان۲]
static int s_away_count;
static float s_weather;

static uint32_t lcg_next(uint32_t *s)
{
    *s = *s * 1664525u + 1013904223u;
    return *s;
}

static float lcg_rand01(uint32_t *s)
{
    return (float)(lcg_next(s) >> 8) / 16777216.0f;
}

// برنامه‌ی یک روز را می‌سازد - قرینه‌ی day_schedule در generate_data.py
static void ensure_day_cache(const struct tm *tm_now)
{
    if (s_cached_yday == tm_now->tm_yday) {
        return;
    }
    s_cached_yday = tm_now->tm_yday;

    // tm_wday: 0=یکشنبه .. 6=شنبه → پنج‌شنبه=4، جمعه=5
    s_cached_weekend = (tm_now->tm_wday == 4 || tm_now->tm_wday == 5);

    uint32_t rs = (uint32_t)tm_now->tm_yday * 2654435761u;
    s_away_count = 0;

    if (s_cached_weekend) {
        float s1 = 9.0f + lcg_rand01(&rs) * 2.0f;
        float s2 = 16.0f + lcg_rand01(&rs) * 2.0f;
        s_away[s_away_count++] = s1;
        s_away[s_away_count++] = s1 + 1.0f + lcg_rand01(&rs) * 2.0f;
        s_away[s_away_count++] = s2;
        s_away[s_away_count++] = s2 + 0.8f + lcg_rand01(&rs) * 1.5f;
    } else {
        float s = 8.0f + lcg_rand01(&rs) * 0.75f;
        s_away[s_away_count++] = s;
        s_away[s_away_count++] = s + 8.5f + lcg_rand01(&rs) * 1.5f;
    }

    s_weather = 0.35f + lcg_rand01(&rs) * 0.65f;

    ESP_LOGI(TAG, "Day plan: %s, away-windows=%d, weather=%.2f",
             s_cached_weekend ? "weekend" : "workday", s_away_count / 2, s_weather);
}

// قرینه‌ی lux_at در generate_data.py
static float lux_at(float t)
{
    float base;
    if (t > VD_SUNRISE_H && t < VD_SUNSET_H) {
        base = 40.0f + 820.0f * s_weather *
               sinf((float)M_PI * (t - VD_SUNRISE_H) / (VD_SUNSET_H - VD_SUNRISE_H));
    } else {
        base = 1.5f;
    }
    // نویز خواندن سنسور
    return fmaxf(0.0f, base * (0.92f + 0.16f * ((float)(esp_random() >> 8) / 16777216.0f)));
}

static bool presence_at(float t)
{
    // باز شدن قفل اخیر = قطعاً خانه
    if (esp_timer_get_time() < s_home_until_us) {
        return true;
    }
    for (int i = 0; i < s_away_count; i += 2) {
        if (t >= s_away[i] && t <= s_away[i + 1]) {
            return false;
        }
    }
    return true;
}

static void virtual_task_fn(void *arg)
{
    while (true) {
        if (time_sync_is_ready()) {
            time_t now = time(NULL);
            struct tm tm_now;
            localtime_r(&now, &tm_now);

            ensure_day_cache(&tm_now);
            float t = (float)tm_now.tm_hour +
                      (float)tm_now.tm_min / 60.0f +
                      (float)tm_now.tm_sec / 3600.0f;

            bool presence = presence_at(t);
            float lux = lux_at(t);

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            bool presence_changed = (presence != s_presence);
            s_presence = presence;
            s_lux = lux;
            s_values_valid = true;
            xSemaphoreGive(s_mutex);

            if (presence_changed) {
                ESP_LOGI(TAG, "Presence -> %s", presence ? "home" : "away");
            }
            mqtt_manager_publish_presence(presence);
            mqtt_manager_publish_lux(lux);

            // همگام‌سازی LCD با entity های Presence/Ambient Light در HA
            if (app_state_lcd_available()) {
                lcd_driver_lvgl_lock();
                ui_update_virtual_env(presence, lux);
                lcd_driver_lvgl_unlock();
            }
        }
        vTaskDelay(pdMS_TO_TICKS(VD_TASK_PERIOD_MS));
    }
}

void virtual_devices_start(void)
{
    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
    }
    xTaskCreate(virtual_task_fn, "virtual_dev", VD_TASK_STACK, NULL,
                VD_TASK_PRIORITY, NULL);
    ESP_LOGI(TAG, "Virtual presence/lux simulator started");
}

void virtual_devices_notify_user_seen(void)
{
    s_home_until_us = esp_timer_get_time() + (int64_t)VD_HOME_FORCE_MS * 1000;

    float lux_now;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_presence = true;
    s_values_valid = true;
    lux_now = s_lux;
    xSemaphoreGive(s_mutex);

    // بازخورد فوری روی LCD (مثلاً از مسیر نتیجه‌ی کیپد در تسک LVGL - قفل
    // بازگشتی است و مشکلی ندارد)
    if (app_state_lcd_available()) {
        lcd_driver_lvgl_lock();
        ui_update_virtual_env(true, lux_now);
        lcd_driver_lvgl_unlock();
    }

    ESP_LOGI(TAG, "User seen (unlock) - presence forced home for 45min");
}

bool virtual_devices_get_env(bool *presence, float *lux)
{
    if (s_mutex == NULL) {
        return false;
    }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    *presence = s_presence;
    *lux = s_lux;
    bool valid = s_values_valid;
    xSemaphoreGive(s_mutex);
    return valid;
}
