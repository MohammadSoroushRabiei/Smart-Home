#include <stdio.h>
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "led.h"
#include "Button.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "lcd_driver.h"
#include "touch_driver.h"
#include "ui_screens.h"
#include "face_recognition.h"
#include "app_state.h"
#include "i2c_bus.h"
#include "bme280.h"
#include "sensor_task.h"
#include "password_manager.h"
#include "keypad_screen.h"
#include "lock.h"

static bool s_lcd_ok = false;

static void on_wifi_state_change(wifi_state_t state)
{
    if (s_lcd_ok) {
        lcd_driver_lvgl_lock();
        ui_update_wifi_status(state);

        if (state == WIFI_STATE_CONNECTED) {
            const char *ip = wifi_get_ip_str();
            char url[48];
            snprintf(url, sizeof(url), "https://%s/capture", ip);

            ui_update_wifi_ip(ip);
            ui_update_capture_qr(url);
        } else {
            ui_update_wifi_ip("");
            ui_update_capture_qr("");
        }

        lcd_driver_lvgl_unlock();
    }
}
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();

    if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
        ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);

    app_state_init();
    ESP_ERROR_CHECK(password_manager_init());

    lock_init();
    
    bool i2c_ok = i2c_bus_init();

    s_lcd_ok = lcd_driver_init();
    app_state_set_lcd_available(s_lcd_ok);

    if (!s_lcd_ok) {
        ESP_LOGW("main", "Continuing without LCD");
    }

    if (s_lcd_ok && i2c_ok) {
        bool touch_ok = touch_driver_init(lcd_driver_get_display(), i2c_bus_get_handle());
        if (!touch_ok) {
            ESP_LOGW("main", "Continuing without touch input");
        }
    }

    if (s_lcd_ok) {
        lcd_driver_lvgl_lock();
        ui_screens_init();
        keypad_screen_init();
        lcd_driver_lvgl_unlock();
    }

    led_init();
    btn_init();

    wifi_register_state_change_cb(on_wifi_state_change);
    wifi_init_sta();  


    ESP_ERROR_CHECK(face_recognition_init());

    if (i2c_ok && bme280_init(i2c_bus_get_handle())) {
        sensor_task_start();
    } else {
        ESP_LOGW("main", "BME280 not found, continuing without sensor data");
    }

    http_server_start();

    while (1)
    {
        if (btn_is_pressed(BTN))
        {
            app_state_set_light(!app_state_get_light());
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}