#include <stdio.h>
#include <stdbool.h>
#include "driver/gpio.h"

#include "led.h"
#include "Button.h"

#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_system.h"
#include "esp_netif.h"















// ----------------------------------------------------- //
//                      HTTP SERVER                      //
// ----------------------------------------------------- //


#include "esp_http_server.h"

static const char *TAG_HTTP = "HTTP";

static esp_err_t led_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG_HTTP, "GET /led received");

    const char *response = led_is_on() ? "LED: ON" : "LED: OFF";

    httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}


static esp_err_t led_toggle_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG_HTTP, "GET /led/toggle received");
    
    led_toggle(LED);
    
    const char *response = led_is_on() ? "LED: ON" : "LED: OFF";
    
    esp_err_t err =  httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
    ESP_LOGI(TAG_HTTP, "httpd_resp_send result: %s", esp_err_to_name(err));

    return err;
}

static esp_err_t root_handler(httpd_req_t * req)
{
    const char *html =
        "<!DOCTYPE html>"
        "<html>"
        "<head>"
        "<title>Smart Home head</title>"
        "</head>"

        "<body>"

        "<h1>Smart Home h1</h1>"

        "<h2 id=\"led-status\">LED:    </h2>"

        "<script>"

        "fetch(\"/led\")"
        ".then(Response => Response.text())"
        ".then(data => {"
        "document.getElementById(\"led-status\").textContent = data;"
        "});"

        "</script>"

        "<button id=\"led-toggle\">Toggle LED</button>"

        "<script>"
        "const btn = document.getElementById(\"led-toggle\");"

        "btn.addEventListener('click',function(){"
            "fetch(\"/led/toggle\")"
            ".then(Response => Response.text())"
            ".then(data => {"
                "document.getElementById(\"led-status\").textContent = data;"
        
            "});"
        "});"

    "</script>"

        "</body>"
        "</html>";

    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);

    return ESP_OK;
}

static const httpd_uri_t led_uri = {
    .uri = "/led" ,
    .method = HTTP_GET ,
    .handler = led_handler ,
    .user_ctx = NULL
} ;


static const httpd_uri_t led_toggle_uri = {
    .uri = "/led/toggle" ,
    .method = HTTP_GET ,
    .handler = led_toggle_handler ,
    .user_ctx = NULL
} ;


static const httpd_uri_t root_uri = {

    .uri = "/" ,
    .method = HTTP_GET ,
    .handler = root_handler ,
    .user_ctx = NULL

};

static httpd_handle_t http_server_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    if (httpd_start(&server,&config) != ESP_OK)
    {
        ESP_LOGI(TAG_HTTP,"Failed to start HTTP Server");
        return NULL;
    }
    httpd_register_uri_handler(server , &led_uri);
    httpd_register_uri_handler(server , &led_toggle_uri);
    httpd_register_uri_handler(server , &root_uri);
    ESP_LOGI(TAG_HTTP,"HTTP Server Started");

    return server;
    
    
}









// ----------------------------------------------------- //
//                          WIFI                         //
// ----------------------------------------------------- //




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
        http_server_start();
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

wifi_state_t wifi_get_state(void)
{
    return wifi_state;
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