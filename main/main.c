#include "nvs_flash.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led.h"
#include "Button.h"
#include "wifi_manager.h"
#include "http_server.h"
#include "face_recognition.h"


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


    led_init();
    btn_init();

    wifi_init_sta();
    
    while (!wifi_is_connected())
    {
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    ESP_ERROR_CHECK(face_recognition_init());
    
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