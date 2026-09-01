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

static bool s_lcd_ok = false;
static void on_wifi_state_change(wifi_state_t state)
{
    if (s_lcd_ok) {
        lcd_driver_lvgl_lock();
        ui_update_wifi_status(state);
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


    s_lcd_ok = lcd_driver_init();
    if (!s_lcd_ok) {
        ESP_LOGW("main", "Continuing without LCD");
    }
    

    if (s_lcd_ok) {
        bool touch_ok = touch_driver_init(lcd_driver_get_display());
        if (!touch_ok) {
            ESP_LOGW("main", "Continuing without touch input");
        }
    }

    if (s_lcd_ok) {
        lcd_driver_lvgl_lock();
        ui_screens_init();
        lcd_driver_lvgl_unlock();
    }

    led_init();
    btn_init();

    wifi_register_state_change_cb(on_wifi_state_change);

    wifi_init_sta();
    
    while (!wifi_is_connected())
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    
    http_server_start();

    while (1)
    {
        if (btn_is_pressed(BTN))
        {
            led_toggle(LED);
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}