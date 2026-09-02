#include "wifi_manager.h"

#include <stdint.h>

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"


#define WIFI_SSID      "MS"
#define WIFI_PASS      "123456789"
#define WIFI_MAX_RETRY 5


static const char *TAG = "WIFI";

static wifi_state_t wifi_state = WIFI_STATE_OFFLINE;
static uint8_t retry_count = 0;
static wifi_state_change_cb_t s_state_change_cb = NULL;

static char s_ip_str[16] = "";   // "255.255.255.255" حداکثر ۱۵ کاراکتر + نال

bool wifi_is_connected(void)
{
    return wifi_state == WIFI_STATE_CONNECTED;
}


wifi_state_t wifi_get_state(void)
{
    return wifi_state;
}

const char *wifi_get_ip_str(void)
{
    return s_ip_str;
}

void wifi_register_state_change_cb(wifi_state_change_cb_t cb)
{
    s_state_change_cb = cb;
}


static const char *wifi_state_to_string(wifi_state_t state)
{
    switch (state)
    {
        case WIFI_STATE_OFFLINE:
            return "OFFLINE";

        case WIFI_STATE_CONNECTING:
            return "CONNECTING";

        case WIFI_STATE_CONNECTED:
            return "CONNECTED";

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

    ESP_LOGI(TAG, "Status: %s", wifi_state_to_string(state));

    if (s_state_change_cb != NULL)
    {
        s_state_change_cb(state);
    }
}


void wifi_retry_connect(void)
{
    ESP_LOGI(TAG, "Manual retry requested");

    if (wifi_state == WIFI_STATE_CONNECTED)
    {
        ESP_LOGI(TAG, "Already connected, nothing to do");
        return;
    }

    retry_count = 0;
    s_ip_str[0] = '\0';
    wifi_set_state(WIFI_STATE_CONNECTING);

    esp_err_t err = esp_wifi_disconnect();   // اطمینان از پاک شدن وضعیت قبلی
    ESP_LOGI(TAG, "disconnect result: %s", esp_err_to_name(err));

    err = esp_wifi_connect();
    ESP_LOGI(TAG, "connect result: %s", esp_err_to_name(err));
}





static void event_handler(
    void *arg,
    esp_event_base_t event_base,
    int32_t event_id,
    void *event_data)
{
    if (event_base == WIFI_EVENT &&
        event_id == WIFI_EVENT_STA_START)
    {
        s_ip_str[0] = '\0';
        wifi_set_state(WIFI_STATE_CONNECTING);

        ESP_ERROR_CHECK(esp_wifi_connect());
    }

    else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        wifi_event_sta_disconnected_t *event =
            (wifi_event_sta_disconnected_t *)event_data;

        ESP_LOGI(
            TAG,
            "SSID: %s / REASON_CODE: %d",
            event->ssid,
            event->reason
        );

        if (retry_count < WIFI_MAX_RETRY)
        {
            retry_count++;

            ESP_LOGI(
                TAG,
                "Retry to connect to the AP (%d / %d)",
                retry_count,
                WIFI_MAX_RETRY
            );
            s_ip_str[0] = '\0';
            wifi_set_state(WIFI_STATE_CONNECTING);

            ESP_ERROR_CHECK(esp_wifi_connect());
        }
        else
        {
            retry_count = 0;

            ESP_LOGI(TAG, "Failed to connect to the AP");
            s_ip_str[0] = '\0';
            wifi_set_state(WIFI_STATE_OFFLINE);
        }
    }

    else if (event_base == IP_EVENT &&
             event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event =
            (ip_event_got_ip_t *)event_data;

        retry_count = 0;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        wifi_set_state(WIFI_STATE_CONNECTED);

        ESP_LOGI(
            TAG,
            "Got IP: " IPSTR,
            IP2STR(&event->ip_info.ip)
        );
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


    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            WIFI_EVENT,
            ESP_EVENT_ANY_ID,
            &event_handler,
            NULL,
            &instance_any_id
        )
    );


    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(
            IP_EVENT,
            IP_EVENT_STA_GOT_IP,
            &event_handler,
            NULL,
            &instance_got_ip
        )
    );


    wifi_config_t wifi_config =
    {
        .sta =
        {
            .ssid = WIFI_SSID,
            .password = WIFI_PASS,
        },
    };


    ESP_ERROR_CHECK(
        esp_wifi_set_mode(WIFI_MODE_STA)
    );

    ESP_ERROR_CHECK(
        esp_wifi_set_config(
            WIFI_IF_STA,
            &wifi_config
        )
    );

    ESP_ERROR_CHECK(
        esp_wifi_start()
    );


    ESP_LOGI(TAG, "WiFi initialization finished.");
}