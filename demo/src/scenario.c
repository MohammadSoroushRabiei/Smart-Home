/**
 * @file scenario.c
 * @brief سناریوی خودکار دمو - یک ترد که قصه را قدم‌به‌قدم با API های واقعی
 *        برد پیش می‌برد (فرمان از HA، باز شدن درب، ML autonomy). هر قدم
 *        از همان مسیرهایی می‌رود که روی برد واقعی می‌رود.
 */
#include <pthread.h>
#include <stdbool.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"

#include "app_state.h"
#include "ml_agent.h"
#include "mqtt_manager.h"
#include "lcd_driver.h"
#include "attendance_screen.h"
#include "scenario.h"

static const char *TAG = "demo_scenario";

static pthread_mutex_t s_started_mtx = PTHREAD_MUTEX_INITIALIZER;
static bool s_started = false;

// ---------------------------------------------------------------------
// سیمولاتور سنسور BME280 - مقادیر نرم متغیر هر ۵ ثانیه
// --------------------------------------------------------------------

static void *sensor_feeder(void *arg)
{
    (void)arg;
    int t = 0;
    for (;;) {
        float temp = 23.0f + 3.5f * (1.0f + (float)__builtin_sinf(t / 12.0f));
        float hum  = 42.0f + 8.0f * (float)__builtin_sinf(t / 17.0f + 1.0f);
        float press = 1012.0f + 2.5f * (float)__builtin_sinf(t / 23.0f + 2.0f);
        app_state_set_sensor_data(temp, hum, press);
        t++;
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    return NULL;
}

void demo_sensor_feeder_start(void)
{
    pthread_t th;
    pthread_create(&th, NULL, sensor_feeder, NULL);
    pthread_detach(th);
}

// ---------------------------------------------------------------------
// گام‌های سناریو
// --------------------------------------------------------------------

static void ha_light_on(void)
{
    ESP_LOGI(TAG, "[HA] smarthome/light/set ON");
    app_state_set_light(true);
}

static void ha_light_off(void)
{
    ESP_LOGI(TAG, "[HA] smarthome/light/set OFF");
    app_state_set_light(false);
}

static void ha_fan_on(void)
{
    ESP_LOGI(TAG, "[HA] smarthome/fan/set ON");
    app_state_set_fan(true);
}

static void ha_fan_off(void)
{
    ESP_LOGI(TAG, "[HA] smarthome/fan/set OFF");
    app_state_set_fan(false);
}

static void face_unlock(void)
{
    // همان مسیر تشخیص چهره روی برد: رویداد access + باز شدن قفل از app_state
    // (خودش "Access Granted" روی LCD نشان می‌دهد و ری‌لاک ۵ ثانیه‌ای می‌گذارد)
    ESP_LOGI(TAG, "[script] known face at the door (Soroush)");
    mqtt_manager_publish_access_event(ACCESS_EVENT_GRANTED_FACE);
    app_state_set_lock(true);
}

static void ml_auto_on(void)
{
    // مثل زدن سوییچ ML Autonomy در Home Assistant
    ESP_LOGI(TAG, "[HA] smarthome/ml/mode/set AUTO");
    ml_agent_set_autonomy(true);
}

static void ml_auto_off(void)
{
    ESP_LOGI(TAG, "[HA] smarthome/ml/mode/set SHADOW");
    ml_agent_set_autonomy(false);
}

static void att_show(void)
{
    ESP_LOGI(TAG, "[script] attendance QR screen");
    lcd_driver_lvgl_lock();
    attendance_screen_show();
    lcd_driver_lvgl_unlock();
}

static void att_hide(void)
{
    lcd_driver_lvgl_lock();
    attendance_screen_hide();
    lcd_driver_lvgl_unlock();
}

typedef struct {
    int delay_ms;                 // فاصله از گام قبلی
    void (*fn)(void);
    const char *desc;
} demo_step_t;

static const demo_step_t s_steps[] = {
    {  4000, ha_light_on,  "HA turns the light ON" },
    { 10000, face_unlock,  "known face at the door - door opens (auto relock 5s)" },
    { 12000, ha_fan_on,    "HA turns the fan ON" },
    { 20000, ml_auto_on,   "ML Autonomy -> AUTO (HA switch)" },
    { 35000, ha_light_off, "HA turns the light OFF (watch ML AUTO react)" },
    { 50000, att_show,     "attendance QR screen" },
    { 58000, att_hide,     NULL },
    { 65000, ha_fan_off,   "HA turns the fan OFF" },
    { 72000, ml_auto_off,  "ML Autonomy -> SHADOW" },
    { 78000, ha_light_on,  "HA turns the light ON (finale)" },
    { 85000, NULL,         NULL },   // پایان سناریو
};

static void *scenario_thread(void *arg)
{
    (void)arg;
    ESP_LOGI(TAG, "auto demo scenario started");
    for (size_t i = 0; i < sizeof(s_steps) / sizeof(s_steps[0]); i++) {
        const demo_step_t *s = &s_steps[i];
        if (s->delay_ms > 0) {
            vTaskDelay(pdMS_TO_TICKS(s->delay_ms));
        }
        if (s->fn == NULL) {
            ESP_LOGI(TAG, "scenario finished");
            break;
        }
        ESP_LOGI(TAG, "step: %s", s->desc);
        s->fn();
    }
    return NULL;
}

void demo_scenario_start(void)
{
    pthread_mutex_lock(&s_started_mtx);
    bool already = s_started;
    s_started = true;
    pthread_mutex_unlock(&s_started_mtx);
    if (already) {
        return;
    }
    pthread_t th;
    pthread_create(&th, NULL, scenario_thread, NULL);
    pthread_detach(th);
}
