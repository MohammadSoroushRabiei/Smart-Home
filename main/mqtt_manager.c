#include "mqtt_manager.h"

#include <string.h>
#include <stdio.h>
#include "mqtt_client.h"
#include "esp_log.h"
#include "app_state.h"

static const char *TAG = "mqtt_manager";

// ===== تنظیمات اتصال (هاردکد فعلاً - مشابه wifi_manager.c) =====
#define MQTT_BROKER_URI  "mqtt://10.64.102.114:1883"

// ===== شناسه‌ی دستگاه (باید در همه‌ی پیام‌های discovery یکسان باشد) =====
#define DEVICE_ID  "esp32_smarthome"

// ===== تاپیک‌های State/Command =====
#define TOPIC_STATUS        "smarthome/status"
#define TOPIC_LIGHT_STATE   "smarthome/light/state"
#define TOPIC_LIGHT_SET     "smarthome/light/set"
#define TOPIC_SENSOR_STATE  "smarthome/sensor/state"
#define TOPIC_ACCESS_STATE  "smarthome/access/state"

// ===== تاپیک‌های Discovery =====
#define DISC_LIGHT   "homeassistant/light/" DEVICE_ID "/light/config"
#define DISC_TEMP    "homeassistant/sensor/" DEVICE_ID "/temperature/config"
#define DISC_HUM     "homeassistant/sensor/" DEVICE_ID "/humidity/config"
#define DISC_PRESS   "homeassistant/sensor/" DEVICE_ID "/pressure/config"
#define DISC_ACCESS  "homeassistant/binary_sensor/" DEVICE_ID "/access/config"

// بلاک مشترک "device" که در همه‌ی پیام‌های discovery تکرار می‌شود
// تا HA بفهمد همه‌ی entity ها متعلق به یک دستگاه واحد هستند
#define DEVICE_BLOCK \
    "\"device\":{" \
    "\"identifiers\":[\"" DEVICE_ID "\"]," \
    "\"name\":\"Smart Home Access Control\"," \
    "\"model\":\"ESP32-S3-DevKitC-1\"," \
    "\"manufacturer\":\"Soroush Rabiei\"" \
    "}"

// بلاک مشترک availability (وابسته به Birth/LWT روی TOPIC_STATUS)
#define AVAILABILITY_BLOCK \
    "\"availability_topic\":\"" TOPIC_STATUS "\"," \
    "\"payload_available\":\"online\"," \
    "\"payload_not_available\":\"offline\""

static esp_mqtt_client_handle_t s_client = NULL;

// ---------------------------------------------------------------------
// Payload های ثابت Discovery (رشته‌های ادغام‌شده در زمان کامپایل)
// ---------------------------------------------------------------------

static const char *s_light_discovery =
    "{"
    "\"name\":\"Light\","
    "\"unique_id\":\"" DEVICE_ID "_light\","
    "\"state_topic\":\"" TOPIC_LIGHT_STATE "\","
    "\"command_topic\":\"" TOPIC_LIGHT_SET "\","
    "\"payload_on\":\"ON\","
    "\"payload_off\":\"OFF\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_temperature_discovery =
    "{"
    "\"name\":\"Temperature\","
    "\"unique_id\":\"" DEVICE_ID "_temperature\","
    "\"state_topic\":\"" TOPIC_SENSOR_STATE "\","
    "\"value_template\":\"{{ value_json.temperature }}\","
    "\"unit_of_measurement\":\"\u00b0C\","
    "\"device_class\":\"temperature\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_humidity_discovery =
    "{"
    "\"name\":\"Humidity\","
    "\"unique_id\":\"" DEVICE_ID "_humidity\","
    "\"state_topic\":\"" TOPIC_SENSOR_STATE "\","
    "\"value_template\":\"{{ value_json.humidity }}\","
    "\"unit_of_measurement\":\"%\","
    "\"device_class\":\"humidity\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_pressure_discovery =
    "{"
    "\"name\":\"Pressure\","
    "\"unique_id\":\"" DEVICE_ID "_pressure\","
    "\"state_topic\":\"" TOPIC_SENSOR_STATE "\","
    "\"value_template\":\"{{ value_json.pressure }}\","
    "\"unit_of_measurement\":\"hPa\","
    "\"device_class\":\"pressure\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_access_discovery =
    "{"
    "\"name\":\"Access Event\","
    "\"unique_id\":\"" DEVICE_ID "_access\","
    "\"state_topic\":\"" TOPIC_ACCESS_STATE "\","
    "\"payload_on\":\"ON\","
    "\"payload_off\":\"OFF\","
    "\"device_class\":\"lock\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

// ---------------------------------------------------------------------
// توابع داخلی
// ---------------------------------------------------------------------

static void publish_discovery_configs(void)
{
    // qos=1 چون گم‌شدن پیام معرفی یعنی entity اصلاً در HA ساخته نمی‌شود
    // retain=true چون HA ممکن است دیرتر از ESP32 بالا بیاید و باید بتواند
    // پیام را بعداً هم از broker بخواند
    esp_mqtt_client_publish(s_client, DISC_LIGHT,  s_light_discovery,     0, 1, true);
    esp_mqtt_client_publish(s_client, DISC_TEMP,   s_temperature_discovery, 0, 1, true);
    esp_mqtt_client_publish(s_client, DISC_HUM,    s_humidity_discovery,  0, 1, true);
    esp_mqtt_client_publish(s_client, DISC_PRESS,  s_pressure_discovery,  0, 1, true);
    esp_mqtt_client_publish(s_client, DISC_ACCESS, s_access_discovery,    0, 1, true);

    ESP_LOGI(TAG, "Discovery configs published for 5 entities");
}

static void handle_light_command(esp_mqtt_event_handle_t event)
{
    bool on = (event->data_len == 2 && memcmp(event->data, "ON", 2) == 0);
    ESP_LOGI(TAG, "Received light command from HA: %s", on ? "ON" : "OFF");

    // نقطه‌ی ورودی مشترک همه‌ی منابع کنترل چراغ (دکمه، LCD، HTTP، حالا MQTT هم)
    app_state_set_light(on);
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "Connected to broker");
        publish_discovery_configs();

        // Birth message - به HA اعلام می‌کند که دستگاه آنلاین است
        esp_mqtt_client_publish(s_client, TOPIC_STATUS, "online", 0, 1, true);

        esp_mqtt_client_subscribe(s_client, TOPIC_LIGHT_SET, 1);
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "Disconnected from broker");
        break;

    case MQTT_EVENT_DATA:
        if (event->topic_len == strlen(TOPIC_LIGHT_SET) &&
            memcmp(event->topic, TOPIC_LIGHT_SET, event->topic_len) == 0) {
            handle_light_command(event);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT error event");
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------
// API عمومی
// ---------------------------------------------------------------------

void mqtt_manager_init(void)
{
    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = MQTT_BROKER_URI,
        // LWT: اگر اتصال به‌طور غیرمنتظره قطع شود (کرش، قطع Wi-Fi)،
        // broker خودش این پیام را به‌جای ESP32 منتشر می‌کند
        .session.last_will.topic = TOPIC_STATUS,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 0,   // 0 یعنی از strlen خود msg استفاده کن
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    if (s_client == NULL) {
        ESP_LOGE(TAG, "Failed to init MQTT client");
        return;
    }

    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, NULL);
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "MQTT client starting (broker: %s)", MQTT_BROKER_URI);
}

void mqtt_manager_publish_light_state(bool on)
{
    if (s_client == NULL) {
        return;
    }
    esp_mqtt_client_publish(s_client, TOPIC_LIGHT_STATE, on ? "ON" : "OFF", 0, 1, true);
}

void mqtt_manager_publish_sensor_state(float temp, float hum, float pressure)
{
    if (s_client == NULL) {
        return;
    }

    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f}",
             temp, hum, pressure);

    esp_mqtt_client_publish(s_client, TOPIC_SENSOR_STATE, payload, 0, 1, false);
}

void mqtt_manager_publish_access_event(bool granted)
{
    if (s_client == NULL) {
        return;
    }
    esp_mqtt_client_publish(s_client, TOPIC_ACCESS_STATE, granted ? "ON" : "OFF", 0, 0, false);
}