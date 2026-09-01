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


    bool lcd_ok = lcd_driver_init();
    if (!lcd_ok) {
        ESP_LOGW("main", "Continuing without LCD");
    }
    

    if (lcd_ok) {
        bool touch_ok = touch_driver_init(lcd_driver_get_display());
        if (!touch_ok) {
            ESP_LOGW("main", "Continuing without touch input");
        }
    }

    lcd_driver_lvgl_lock();
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello Smart Home");
    lv_obj_center(label);
    lcd_driver_lvgl_unlock();

    led_init();
    btn_init();

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