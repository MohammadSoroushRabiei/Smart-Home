#include "app_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "led.h"
#include "lcd_driver.h"
#include "ui_screens.h"

static const char *TAG = "app_state";

static SemaphoreHandle_t s_mutex;
static bool s_light_on = false;
static bool s_lcd_available = false;   // ← جدید

void app_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
}

// ← تابع جدید: باید بعد از مشخص شدن نتیجه‌ی lcd_driver_init صدا زده شود
void app_state_set_lcd_available(bool available)
{
    s_lcd_available = available;
}

bool app_state_get_light(void)
{
    bool value;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    value = s_light_on;
    xSemaphoreGive(s_mutex);
    return value;
}

void app_state_set_light(bool on)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_light_on = on;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Light set to: %s", on ? "ON" : "OFF");

    led_set(LED, on);

    if (s_lcd_available) {
        lcd_driver_lvgl_lock();
        ui_update_light_status(on);
        lcd_driver_lvgl_unlock();
    }
}