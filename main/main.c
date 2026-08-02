#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"


#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_netif.h"



#define BTN GPIO_NUM_1
#define LED GPIO_NUM_2

bool led_state = false;
bool btn_last_state = false;

static const char * TAG_LED = "LED Status";


void led_init()
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
}

void btn_init()
{
    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN, GPIO_PULLDOWN_ONLY);
}

bool btn_is_pressed(gpio_num_t btn)
{
    bool btn_current_state = gpio_get_level(btn);
        
    if (btn_current_state != btn_last_state) {
        vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        btn_current_state = gpio_get_level(btn);
        btn_last_state = btn_current_state ;

        return btn_current_state;
    }
    
    return false;
}


void led_toggle(gpio_num_t led)
{
    led_state = !led_state ;
    gpio_set_level(led,led_state);
    ESP_LOGI(TAG_LED , "%s" , led_state ? "ON" : "OFF");
}









#define WIFI_SSID      "MS"
#define WIFI_PASS      "123456789"
#define WIFI_MAX_RETRY 5

static const char *TAG = "WIFI";
static uint8_t retry_count = 0;

typedef enum{
    WIFI_STATE_OFFLINE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED
} wifi_state_t ;

static wifi_state_t wifi_state = WIFI_STATE_OFFLINE;

bool wifi_is_connected(void)
{
    return wifi_state == WIFI_STATE_CONNECTED;
}

static const char *wifi_state_to_string(wifi_state_t state)
{
    
    switch (state)
    {
    case WIFI_STATE_OFFLINE:
        return "OFFLINE" ;
    
    case WIFI_STATE_CONNECTING:
        return "CONNECTING" ;
    
    case WIFI_STATE_CONNECTED:    
        return "CONNECTED" ;
    
    default:
        return "ERROR";
    }


}
static void wifi_set_state(wifi_state_t state)
{
    
    if (wifi_state == state)
    {
        return;
    }

    wifi_state = state;   
    ESP_LOGI(TAG, "Status : %s" , wifi_state_to_string(state));
    
}

wifi_state_t wifi_get_state(void)
{
    return wifi_state;
}

static void event_handler(void* arg, esp_event_base_t event_base,
    int32_t event_id, void* event_data)
    {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        wifi_set_state(WIFI_STATE_CONNECTING);
        esp_wifi_connect();
    } 
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {

        wifi_event_sta_disconnected_t * event_disconnected_data = ( wifi_event_sta_disconnected_t *) event_data;

        ESP_LOGI(TAG, "SSID : %s / REASON_CODE : %d",(event_disconnected_data->ssid),(event_disconnected_data->reason));

        if (retry_count < WIFI_MAX_RETRY)
        {
            retry_count++;
            ESP_LOGI(TAG, "retry to connect to the AP (%d / %d)",retry_count,WIFI_MAX_RETRY);
            wifi_set_state(WIFI_STATE_CONNECTING);
            esp_wifi_connect();
        }
        else
        {
            retry_count = 0 ;
            ESP_LOGI(TAG, "Failed to connect to the AP");
            wifi_set_state(WIFI_STATE_OFFLINE);
        }
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        wifi_set_state(WIFI_STATE_CONNECTED);
        retry_count = 0 ;
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    }
}

void wifi_init_sta(void)
{

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
            /* Authmode threshold resets to WPA2 as default if password matches WPA2 standards (password len => 8).
             * If you want to connect the device to deprecated WEP/WPA networks, Please set the threshold value
             * to WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK and set the password with length and format matching to
             * WIFI_AUTH_WEP/WIFI_AUTH_WPA_PSK standards.
             */

        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
}




void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);


    led_init();
    btn_init();

    wifi_init_sta();
    

    
    while (1)
    {
        if(btn_is_pressed(BTN))
        {
            led_toggle(LED);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}