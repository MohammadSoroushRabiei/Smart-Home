#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "led.h"
#include "lcd_driver.h"
#include "ui_screens.h"
#include "mqtt_manager.h"
#include "lock.h"
#include "esp_timer.h"
#include "keypad_screen.h"
#include "virtual_devices.h"

static const char *TAG = "app_state";

static SemaphoreHandle_t s_mutex;
static bool s_light_on = false;
static bool s_fan_on = false;
static bool s_lock_unlocked = false;
static bool s_lcd_available = false;
static sensor_data_t s_sensor_data = { .valid = false };
static esp_timer_handle_t s_relock_timer = NULL;
static app_state_device_cb_t s_device_cb = NULL;

static void notify_device_change(app_device_t dev, bool new_state,
                                 bool prev_state, bool from_ml)
{
    if (s_device_cb != NULL) {
        s_device_cb(dev, new_state, prev_state, from_ml);
    }
}

static void relock_timer_cb(void *arg)
{
    app_state_set_lock(false);
}

void app_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    const esp_timer_create_args_t timer_args = {
        .callback = &relock_timer_cb,
        .name = "app_relock",
    };
    esp_timer_create(&timer_args, &s_relock_timer);
}
// ← تابع جدید: باید بعد از مشخص شدن نتیجه‌ی lcd_driver_init صدا زده شود
void app_state_set_lcd_available(bool available)
{
    s_lcd_available = available;
}

bool app_state_lcd_available(void)
{
    return s_lcd_available;
}

void app_state_register_device_cb(app_state_device_cb_t cb)
{
    s_device_cb = cb;
}

bool app_state_get_light(void)
{
    bool value;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    value = s_light_on;
    xSemaphoreGive(s_mutex);
    return value;
}

// هسته‌ی مشترک تغییر چراغ - فقط همین‌جا قفل/MQTT/LCD/observer را لمس می‌کنیم.
// observer *بعد از* آزادشدن s_mutex صدا زده می‌شود چون داخل ml_agent دوباره
// getterهای app_state را می‌گیرد (mutex غیربازگشتی است).
static void set_light_core(bool on, bool from_ml)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool prev = s_light_on;
    s_light_on = on;
    xSemaphoreGive(s_mutex);

    if (prev == on) {
        return;
    }

    ESP_LOGI(TAG, "Light set to: %s (%s)", on ? "ON" : "OFF",
             from_ml ? "ML" : "user");

    led_set(LED, on);
    mqtt_manager_publish_light_state(on);

    if (s_lcd_available) {
        lcd_driver_lvgl_lock();
        ui_update_light_status(on);
        lcd_driver_lvgl_unlock();
    }

    notify_device_change(APP_DEV_LIGHT, on, prev, from_ml);
}

void app_state_set_light(bool on)
{
    set_light_core(on, false);
}

void app_state_set_light_auto(bool on)
{
    set_light_core(on, true);
}

bool app_state_get_fan(void)
{
    bool value;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    value = s_fan_on;
    xSemaphoreGive(s_mutex);
    return value;
}

// فن فعلاً مجازی است (بدون GPIO/رله) - وضعیت + publish + UI + observer.
// با اتصال رله واقعی، همین‌جا set_level اضافه می‌شود.
static void set_fan_core(bool on, bool from_ml)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool prev = s_fan_on;
    s_fan_on = on;
    xSemaphoreGive(s_mutex);

    if (prev == on) {
        return;
    }

    ESP_LOGI(TAG, "Fan set to: %s (%s)", on ? "ON" : "OFF",
             from_ml ? "ML" : "user");

    mqtt_manager_publish_fan_state(on);

    if (s_lcd_available) {
        lcd_driver_lvgl_lock();
        ui_update_fan_status(on);
        lcd_driver_lvgl_unlock();
    }

    notify_device_change(APP_DEV_FAN, on, prev, from_ml);
}

void app_state_set_fan(bool on)
{
    set_fan_core(on, false);
}

void app_state_set_fan_auto(bool on)
{
    set_fan_core(on, true);
}


void app_state_set_sensor_data(float temp, float hum, float pressure)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_sensor_data.temperature_c    = temp;
    s_sensor_data.humidity_percent = hum;
    s_sensor_data.pressure_hpa     = pressure;
    s_sensor_data.valid = true;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Sensor: T=%.1fC H=%.1f%% P=%.1fhPa", temp, hum, pressure);

    if (s_lcd_available) {
        lcd_driver_lvgl_lock();
        ui_update_sensor_status(temp, hum, pressure);
        lcd_driver_lvgl_unlock();
    }
}

sensor_data_t app_state_get_sensor_data(void)
{
    sensor_data_t value;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    value = s_sensor_data;
    xSemaphoreGive(s_mutex);
    return value;
}

bool app_state_get_lock(void)
{
    bool value;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    value = s_lock_unlocked;
    xSemaphoreGive(s_mutex);
    return value;
}


#define APP_LOCK_AUTO_RELOCK_MS   5000   // باید کمتر از LOCK_HW_FAILSAFE_MS در lock.h باشد

void app_state_set_lock(bool unlocked)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool prev = s_lock_unlocked;
    s_lock_unlocked = unlocked;
    xSemaphoreGive(s_mutex);

    if (prev == unlocked) {
        return;
    }

    ESP_LOGI(TAG, "Lock set to: %s", unlocked ? "UNLOCKED" : "LOCKED");

    lock_set(unlocked);
    mqtt_manager_publish_lock_state(unlocked);

    // هر باز شدن موفق (چهره یا رمز) یعنی کاربر رسیده - سیگنال حضور واقعی
    // برای ماژول حضور مجازی و مدل ML
    if (unlocked) {
        virtual_devices_notify_user_seen();
    }

    if (s_lcd_available) {                          // ← بلوک جدید
        lcd_driver_lvgl_lock();
        ui_update_lock_status(unlocked);
        if (unlocked) {
            keypad_screen_grant_access();
        }
        lcd_driver_lvgl_unlock();
    }

    if (s_relock_timer) {
        esp_timer_stop(s_relock_timer);
        if (unlocked) {
            esp_timer_start_once(s_relock_timer, (uint64_t)APP_LOCK_AUTO_RELOCK_MS * 1000);
        }
    }
}