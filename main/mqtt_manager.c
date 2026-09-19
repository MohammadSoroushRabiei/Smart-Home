#include "mqtt_manager.h"
#include "mqtt_config.h"
#include "wifi_manager.h"
#include <string.h>
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "mqtt_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "app_state.h"
#include "ml_agent.h"

static const char *TAG = "mqtt_manager";

#define DEVICE_ID  "esp32_smarthome"

#define MQTT_MAX_RETRIES         3
#define MQTT_RETRY_DELAY_MS      3000
#define MQTT_BROKER_PORT         1883
// Mosquitto بعد از ۱٫۵ برابر این مقدار (~۲۲ ثانیه) LWT را منتشر می‌کند؛
// پیش‌فرض esp-mqtt ۱۲۰ ثانیه است (یعنی ~۳ دقیقه تا HA بفهمد).
#define MQTT_KEEPALIVE_S         15

#define MQTT_NETWORK_TIMEOUT_MS   3000
#define MQTT_TEST_WAIT_TIMEOUT_MS (MQTT_NETWORK_TIMEOUT_MS + 500)

#define TOPIC_STATUS        "smarthome/status"
#define TOPIC_LIGHT_STATE   "smarthome/light/state"
#define TOPIC_LIGHT_SET     "smarthome/light/set"
#define TOPIC_FAN_STATE     "smarthome/fan/state"
#define TOPIC_FAN_SET       "smarthome/fan/set"
#define TOPIC_SENSOR_STATE  "smarthome/sensor/state"
#define TOPIC_ACCESS_STATE  "smarthome/access/state"
#define TOPIC_LOCK_STATE   "smarthome/lock/state"
#define TOPIC_PRESENCE      "smarthome/presence/state"
#define TOPIC_LUX           "smarthome/lux/state"
#define TOPIC_ML_MODE       "smarthome/ml/mode"
#define TOPIC_ML_MODE_SET   "smarthome/ml/mode/set"
#define TOPIC_ML_STATS      "smarthome/ml/stats"

#define DISC_LIGHT   "homeassistant/light/" DEVICE_ID "/light/config"
#define DISC_TEMP    "homeassistant/sensor/" DEVICE_ID "/temperature/config"
#define DISC_HUM     "homeassistant/sensor/" DEVICE_ID "/humidity/config"
#define DISC_PRESS   "homeassistant/sensor/" DEVICE_ID "/pressure/config"
#define DISC_ACCESS  "homeassistant/event/" DEVICE_ID "/access/config"
#define DISC_LOCK_STATUS   "homeassistant/binary_sensor/" DEVICE_ID "/lock_status/config"
#define DISC_FAN           "homeassistant/fan/" DEVICE_ID "/fan/config"
#define DISC_PRESENCE      "homeassistant/binary_sensor/" DEVICE_ID "/presence/config"
#define DISC_LUX           "homeassistant/sensor/" DEVICE_ID "/lux/config"
#define DISC_ML_SWITCH     "homeassistant/switch/" DEVICE_ID "/ml_autonomy/config"
#define DISC_ML_STATUS     "homeassistant/sensor/" DEVICE_ID "/ml_agent/config"
#define DISC_ML_PLIGHT     "homeassistant/sensor/" DEVICE_ID "/ml_p_light/config"
#define DISC_ML_PFAN       "homeassistant/sensor/" DEVICE_ID "/ml_p_fan/config"

#define DEVICE_BLOCK \
    "\"device\":{" \
    "\"identifiers\":[\"" DEVICE_ID "\"]," \
    "\"name\":\"Smart Home Access Control\"," \
    "\"model\":\"ESP32-S3-DevKitC-1\"," \
    "\"manufacturer\":\"Soroush Rabiei\"" \
    "}"

#define AVAILABILITY_BLOCK \
    "\"availability_topic\":\"" TOPIC_STATUS "\"," \
    "\"payload_available\":\"online\"," \
    "\"payload_not_available\":\"offline\""

static esp_mqtt_client_handle_t s_client = NULL;
static volatile bool s_mqtt_connected = false;
static bool s_client_started = false;

static mqtt_manager_state_t s_state = MQTT_MGR_STATE_UNCONFIGURED;
static mqtt_manager_state_change_cb_t s_state_cb = NULL;
static esp_timer_handle_t s_retry_timer = NULL;
static uint8_t s_retry_count = 0;
static char s_broker_host[MQTT_CONFIG_HOST_MAX_LEN + 1] = "";

static bool s_user_disabled = false;

static esp_mqtt_client_handle_t s_test_client = NULL;
static SemaphoreHandle_t s_test_sem = NULL;
static volatile bool s_test_success = false;

// ---------------------------------------------------------------------
// Payload های ثابت Discovery
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
    "\"event_types\":[\"access_granted_face\",\"access_denied_face\",\"access_granted_code\",\"access_denied_code\"],"
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_lock_status_discovery =
    "{"
    "\"name\":\"Door Lock Status\","
    "\"unique_id\":\"" DEVICE_ID "_lock_status\","
    "\"state_topic\":\"" TOPIC_LOCK_STATE "\","
    "\"payload_on\":\"UNLOCKED\","
    "\"payload_off\":\"LOCKED\","
    "\"device_class\":\"lock\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_fan_discovery =
    "{"
    "\"name\":\"Fan\","
    "\"unique_id\":\"" DEVICE_ID "_fan\","
    "\"state_topic\":\"" TOPIC_FAN_STATE "\","
    "\"command_topic\":\"" TOPIC_FAN_SET "\","
    "\"payload_on\":\"ON\","
    "\"payload_off\":\"OFF\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_presence_discovery =
    "{"
    "\"name\":\"Presence\","
    "\"unique_id\":\"" DEVICE_ID "_presence\","
    "\"state_topic\":\"" TOPIC_PRESENCE "\","
    "\"payload_on\":\"HOME\","
    "\"payload_off\":\"AWAY\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_lux_discovery =
    "{"
    "\"name\":\"Ambient Light\","
    "\"unique_id\":\"" DEVICE_ID "_lux\","
    "\"state_topic\":\"" TOPIC_LUX "\","
    "\"value_template\":\"{{ value_json.lux }}\","
    "\"unit_of_measurement\":\"lx\","
    "\"device_class\":\"illuminance\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

// سوییچ ML Autonomy: فرمان AUTO/SHADOW → ml_agent_set_autonomy
static const char *s_ml_switch_discovery =
    "{"
    "\"name\":\"ML Autonomy\","
    "\"unique_id\":\"" DEVICE_ID "_ml_autonomy\","
    "\"state_topic\":\"" TOPIC_ML_MODE "\","
    "\"command_topic\":\"" TOPIC_ML_MODE_SET "\","
    "\"payload_on\":\"AUTO\","
    "\"payload_off\":\"SHADOW\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

// سنسور وضعیت عامل: state = AUTO/SHADOW + بقیه به‌عنوان attribute
static const char *s_ml_status_discovery =
    "{"
    "\"name\":\"ML Agent\","
    "\"unique_id\":\"" DEVICE_ID "_ml_agent\","
    "\"state_topic\":\"" TOPIC_ML_STATS "\","
    "\"value_template\":\"{% if value_json.auto_light or value_json.auto_fan %}AUTO{% else %}SHADOW{% endif %}\","
    "\"json_attributes_topic\":\"" TOPIC_ML_STATS "\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_ml_p_light_discovery =
    "{"
    "\"name\":\"ML Light Probability\","
    "\"unique_id\":\"" DEVICE_ID "_ml_p_light\","
    "\"state_topic\":\"" TOPIC_ML_STATS "\","
    "\"value_template\":\"{{ (value_json.p_light * 100) | round(0) }}\","
    "\"unit_of_measurement\":\"%\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

static const char *s_ml_p_fan_discovery =
    "{"
    "\"name\":\"ML Fan Probability\","
    "\"unique_id\":\"" DEVICE_ID "_ml_p_fan\","
    "\"state_topic\":\"" TOPIC_ML_STATS "\","
    "\"value_template\":\"{{ (value_json.p_fan * 100) | round(0) }}\","
    "\"unit_of_measurement\":\"%\","
    AVAILABILITY_BLOCK ","
    DEVICE_BLOCK
    "}";

// ---------------------------------------------------------------------
// مدیریت وضعیت
// ---------------------------------------------------------------------

static void set_state(mqtt_manager_state_t new_state)
{
    if (s_state == new_state) {
        return;
    }
    s_state = new_state;
    if (s_state_cb != NULL) {
        s_state_cb(new_state);
    }
}

static void retry_timer_cb(void *arg)
{
    if (s_client == NULL || !s_client_started) {
        return;
    }
    // اگر شبکه هنوز برنگشته (مثلاً حین اسکن WiFi یا قطعی لحظه‌ای)، تلاش نسوز:
    // قبلاً retryها درست در پنجره‌ی بی‌شبکه مصرف می‌شدند (esp-tls: Host is
    // unreachable) و «Max retries» می‌توانست MQTT را تا ریبوت بعدی خاموش کند.
    // تایمر بدون مصرف شمارنده عقب می‌افتد؛ برگشت شبکه را هم
    // mqtt_manager_notify_network_available پوشش می‌دهد.
    if (!wifi_is_connected()) {
        ESP_LOGI(TAG, "Network not back yet - postponing MQTT retry (attempt %d/%d not consumed)",
                 s_retry_count + 1, MQTT_MAX_RETRIES);
        esp_timer_start_once(s_retry_timer, (uint64_t)MQTT_RETRY_DELAY_MS * 1000);
        return;
    }
    ESP_LOGI(TAG, "Retrying MQTT connection (attempt %d/%d)", s_retry_count, MQTT_MAX_RETRIES);
    set_state(MQTT_MGR_STATE_CONNECTING);
    esp_mqtt_client_reconnect(s_client);
}

// ---------------------------------------------------------------------
// توابع داخلی
// ---------------------------------------------------------------------

static const char *access_event_type_to_str(access_event_type_t type)
{
    switch (type) {
        case ACCESS_EVENT_GRANTED_FACE: return "access_granted_face";
        case ACCESS_EVENT_DENIED_FACE:  return "access_denied_face";
        case ACCESS_EVENT_GRANTED_CODE: return "access_granted_code";
        case ACCESS_EVENT_DENIED_CODE:  return "access_denied_code";
        default:                       return "unknown";
    }
}

static void publish_discovery_configs(void)
{
    esp_mqtt_client_enqueue(s_client, DISC_LIGHT,  s_light_discovery,     0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_TEMP,   s_temperature_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_HUM,    s_humidity_discovery,  0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_PRESS,  s_pressure_discovery,  0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_ACCESS, s_access_discovery,    0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_LOCK_STATUS, s_lock_status_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_FAN, s_fan_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_PRESENCE, s_presence_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_LUX, s_lux_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_ML_SWITCH, s_ml_switch_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_ML_STATUS, s_ml_status_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_ML_PLIGHT, s_ml_p_light_discovery, 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, DISC_ML_PFAN, s_ml_p_fan_discovery, 0, 1, true, true);

    ESP_LOGI(TAG, "Discovery configs enqueued for 13 entities");
}

static void on_main_client_connected(void)
{
    publish_discovery_configs();
    esp_mqtt_client_enqueue(s_client, TOPIC_STATUS, "online", 0, 1, true, true);

    // همگام‌سازی وضعیت واقعی دستگاه‌ها با HA: retained های قدیمی ممکن است
    // بعد از ریست برد با وضعیت فعلی فرق داشته باشند
    esp_mqtt_client_enqueue(s_client, TOPIC_LIGHT_STATE,
                            app_state_get_light() ? "ON" : "OFF", 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, TOPIC_FAN_STATE,
                            app_state_get_fan() ? "ON" : "OFF", 0, 1, true, true);
    esp_mqtt_client_enqueue(s_client, TOPIC_ML_MODE,
                            ml_agent_any_auto() ? "AUTO" : "SHADOW", 0, 1, true, true);

    esp_mqtt_client_subscribe(s_client, TOPIC_LIGHT_SET, 1);
    esp_mqtt_client_subscribe(s_client, TOPIC_FAN_SET, 1);
    esp_mqtt_client_subscribe(s_client, TOPIC_ML_MODE_SET, 1);
}

static void handle_light_command(esp_mqtt_event_handle_t event)
{
    bool on = (event->data_len == 2 && memcmp(event->data, "ON", 2) == 0);
    ESP_LOGI(TAG, "Received light command from HA: %s", on ? "ON" : "OFF");
    app_state_set_light(on);
}

static void handle_fan_command(esp_mqtt_event_handle_t event)
{
    bool on = (event->data_len == 2 && memcmp(event->data, "ON", 2) == 0);
    ESP_LOGI(TAG, "Received fan command from HA: %s", on ? "ON" : "OFF");
    app_state_set_fan(on);
}

static void handle_ml_mode_command(esp_mqtt_event_handle_t event)
{
    if (event->data_len == 4 && memcmp(event->data, "AUTO", 4) == 0) {
        ESP_LOGI(TAG, "Received ML autonomy command from HA: AUTO");
        ml_agent_set_autonomy(true);
    } else if (event->data_len == 6 && memcmp(event->data, "SHADOW", 6) == 0) {
        ESP_LOGI(TAG, "Received ML autonomy command from HA: SHADOW");
        ml_agent_set_autonomy(false);
    } else {
        ESP_LOGW(TAG, "Unknown ML mode payload (%d bytes)", event->data_len);
    }
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                                int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;
    esp_mqtt_client_handle_t event_client = (esp_mqtt_client_handle_t)handler_args;
    bool is_test = (event_client == s_test_client);
    bool is_main = (event_client == s_client);

    switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
        if (is_test) {
            s_test_success = true;
            xSemaphoreGive(s_test_sem);
            break;
        }
        if (is_main) {
            ESP_LOGI(TAG, "Connected to broker");
            s_mqtt_connected = true;
            s_retry_count = 0;
            esp_timer_stop(s_retry_timer);
            set_state(MQTT_MGR_STATE_CONNECTED);
            on_main_client_connected();
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        if (is_test) {
            s_test_success = false;
            xSemaphoreGive(s_test_sem);
            break;
        }
        if (is_main) {
            ESP_LOGW(TAG, "Disconnected from broker");
            s_mqtt_connected = false;

            if (!s_client_started) {
                break;   // نتیجه‌ی توقف دستی است - وارد retry نشو
            }

            if (s_retry_count < MQTT_MAX_RETRIES) {
                s_retry_count++;
                set_state(MQTT_MGR_STATE_CONNECTING);
                esp_timer_stop(s_retry_timer);
                esp_timer_start_once(s_retry_timer, (uint64_t)MQTT_RETRY_DELAY_MS * 1000);
            } else {
                ESP_LOGW(TAG, "Max MQTT retries reached, going offline");
                set_state(MQTT_MGR_STATE_DISABLED);
            }
        }
        break;

    case MQTT_EVENT_DATA:
        if (is_main) {
            if (event->topic_len == strlen(TOPIC_LIGHT_SET) &&
                memcmp(event->topic, TOPIC_LIGHT_SET, event->topic_len) == 0) {
                handle_light_command(event);
            } else if (event->topic_len == strlen(TOPIC_FAN_SET) &&
                       memcmp(event->topic, TOPIC_FAN_SET, event->topic_len) == 0) {
                handle_fan_command(event);
            } else if (event->topic_len == strlen(TOPIC_ML_MODE_SET) &&
                       memcmp(event->topic, TOPIC_ML_MODE_SET, event->topic_len) == 0) {
                handle_ml_mode_command(event);
            }
        }
        break;

    case MQTT_EVENT_ERROR:
        if (is_test) {
            s_test_success = false;
            xSemaphoreGive(s_test_sem);
            break;
        }
        if (is_main) {
            ESP_LOGE(TAG, "MQTT error event");
            s_mqtt_connected = false;
        }
        break;

    default:
        break;
    }
}

static esp_mqtt_client_config_t build_client_config(const char *uri)
{
    esp_mqtt_client_config_t cfg = {
        .broker.address.uri = uri,
        .network.disable_auto_reconnect = true,
        .network.timeout_ms = MQTT_NETWORK_TIMEOUT_MS,
        .session.keepalive = MQTT_KEEPALIVE_S,
        .session.last_will.topic = TOPIC_STATUS,
        .session.last_will.msg = "offline",
        .session.last_will.msg_len = 0,
        .session.last_will.qos = 1,
        .session.last_will.retain = true,
    };
    return cfg;
}

static void start_client(const char *host)
{
    if (s_client == NULL) {
        char uri[96];
        snprintf(uri, sizeof(uri), "mqtt://%s:%d", host, MQTT_BROKER_PORT);

        esp_mqtt_client_config_t mqtt_cfg = build_client_config(uri);
        s_client = esp_mqtt_client_init(&mqtt_cfg);
        if (s_client == NULL) {
            ESP_LOGE(TAG, "Failed to init MQTT client");
            set_state(MQTT_MGR_STATE_DISABLED);
            return;
        }
        esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, (void *)s_client);
    }

    s_retry_count = 0;
    s_mqtt_connected = false;
    esp_timer_stop(s_retry_timer);
    set_state(MQTT_MGR_STATE_CONNECTING);

    // ⚠️ باید قبل از start ست شود - وگرنه یک شکست فوری (مثلاً شبکه هنوز
    // آماده نیست) توسط handler به‌اشتباه «توقف دستی» تفسیر می‌شود
    s_client_started = true;
    esp_mqtt_client_start(s_client);

    ESP_LOGI(TAG, "MQTT client starting (broker: %s)", host);
}

static void stop_client_with_offline_publish(void)
{
    if (s_client == NULL || !s_client_started) {
        return;
    }
    esp_timer_stop(s_retry_timer);
    if (s_mqtt_connected) {
        esp_mqtt_client_publish(s_client, TOPIC_STATUS, "offline", 0, 1, true);
    }
    s_client_started = false;   // قبل از stop: تا DISCONNECTED حین توقف وارد retry نشود
    esp_mqtt_client_stop(s_client);
    s_mqtt_connected = false;
    s_retry_count = 0;
}

// تسک پس‌زمینه‌ای که آماده‌سازی برای قطع (publish آفلاین + stop) را انجام
// می‌دهد و خودش را پاک می‌کند - برای فراخوانی از یک callback LVGL که نباید
// خودش بلاک بشود (مثل تپ روی دکمه‌ی HA).
static void disable_task(void *arg)
{
    mqtt_manager_prepare_for_network_loss();
    vTaskDelete(NULL);
}

// ---------------------------------------------------------------------
// API عمومی
// ---------------------------------------------------------------------

void mqtt_manager_register_state_change_cb(mqtt_manager_state_change_cb_t cb)
{
    s_state_cb = cb;
}

mqtt_manager_state_t mqtt_manager_get_state(void)
{
    return s_state;
}

void mqtt_manager_init(void)
{
    if (s_retry_timer == NULL) {
        const esp_timer_create_args_t retry_timer_args = {
            .callback = &retry_timer_cb,
            .name = "mqtt_retry",
        };
        esp_timer_create(&retry_timer_args, &s_retry_timer);
    }
    if (s_test_sem == NULL) {
        s_test_sem = xSemaphoreCreateBinary();
    }

    s_user_disabled = false;

    if (!mqtt_config_get_host(s_broker_host, sizeof(s_broker_host))) {
        ESP_LOGI(TAG, "No MQTT broker configured yet - staying unconfigured");
        set_state(MQTT_MGR_STATE_UNCONFIGURED);
        return;
    }

    if (!wifi_is_connected()) {
        // WiFi هنوز وصل نشده (wifi_manager_enable غیربلاک‌کننده است) - منتظر
        // اعلان mqtt_manager_notify_network_available می‌مانیم، مثل WiFi
        ESP_LOGI(TAG, "MQTT broker configured but WiFi not connected yet - waiting");
        set_state(MQTT_MGR_STATE_DISABLED);
        return;
    }

    start_client(s_broker_host);
}

bool mqtt_manager_connect_and_save(const char *host)
{
    if (host == NULL || host[0] == '\0') {
        return false;
    }

    if (s_state == MQTT_MGR_STATE_CONNECTED && strcmp(host, s_broker_host) == 0) {
        return true;
    }

    char uri[96];
    snprintf(uri, sizeof(uri), "mqtt://%s:%d", host, MQTT_BROKER_PORT);

    esp_mqtt_client_config_t test_cfg = build_client_config(uri);
    esp_mqtt_client_handle_t test_client = esp_mqtt_client_init(&test_cfg);
    if (test_client == NULL) {
        return false;
    }
    esp_mqtt_client_register_event(test_client, ESP_EVENT_ANY_ID, mqtt_event_handler, (void *)test_client);

    xSemaphoreTake(s_test_sem, 0);
    s_test_success = false;
    s_test_client = test_client;

    esp_mqtt_client_start(test_client);

    BaseType_t got_signal = xSemaphoreTake(s_test_sem, pdMS_TO_TICKS(MQTT_TEST_WAIT_TIMEOUT_MS));
    s_test_client = NULL;

    bool success = (got_signal == pdTRUE) && s_test_success;

    if (!success) {
        ESP_LOGW(TAG, "Test connection to '%s' failed", uri);
        esp_mqtt_client_stop(test_client);
        esp_mqtt_client_destroy(test_client);
        return false;
    }

    if (s_client != NULL) {
        esp_timer_stop(s_retry_timer);
        if (s_mqtt_connected) {
            esp_mqtt_client_publish(s_client, TOPIC_STATUS, "offline", 0, 1, true);
        }
        esp_mqtt_client_stop(s_client);
        esp_mqtt_client_destroy(s_client);
    }

    s_client = test_client;
    s_client_started = true;
    s_mqtt_connected = true;
    s_retry_count = 0;
    s_user_disabled = false;

    mqtt_config_set_host(host);
    strncpy(s_broker_host, host, sizeof(s_broker_host) - 1);
    s_broker_host[sizeof(s_broker_host) - 1] = '\0';

    set_state(MQTT_MGR_STATE_CONNECTED);
    on_main_client_connected();

    ESP_LOGI(TAG, "Switched MQTT broker to: %s", uri);
    return true;
}

void mqtt_manager_enable(void)
{
    if (s_broker_host[0] == '\0') {
        return;   // هنوز هیچ بروکری تنظیم نشده
    }
    if (!wifi_is_connected()) {
        ESP_LOGW(TAG, "Cannot enable MQTT - WiFi is not connected");
        return;   // دکمه باید خاکستری بماند، نه نارنجی
    }
    ESP_LOGI(TAG, "Manual MQTT enable requested");
    s_user_disabled = false;
    start_client(s_broker_host);
}

void mqtt_manager_disable(void)
{
    if (s_client == NULL || !s_client_started) {
        return;
    }
    ESP_LOGI(TAG, "MQTT manually disabled");
    s_user_disabled = true;
    xTaskCreate(disable_task, "mqtt_disable_task", 4096, NULL, 3, NULL);
}

void mqtt_manager_prepare_for_network_loss(void)
{
    if (s_client == NULL || !s_client_started) {
        return;
    }
    stop_client_with_offline_publish();
    set_state(MQTT_MGR_STATE_DISABLED);
}

void mqtt_manager_notify_network_lost(void)
{
    if (s_client == NULL || !s_client_started) {
        return;
    }
    ESP_LOGI(TAG, "Network lost - stopping MQTT (will auto-retry once network is back)");
    xTaskCreate(disable_task, "mqtt_netlost_task", 4096, NULL, 3, NULL);
    // s_user_disabled دست‌نخورده می‌ماند
}

void mqtt_manager_notify_network_available(void)
{
    if (s_user_disabled) {
        return;
    }
    if (s_broker_host[0] == '\0') {
        return;
    }
    if (s_state == MQTT_MGR_STATE_CONNECTED || s_state == MQTT_MGR_STATE_CONNECTING) {
        return;
    }

    ESP_LOGI(TAG, "Network available - auto-reconnecting to last known MQTT broker");
    start_client(s_broker_host);
}

void mqtt_manager_publish_light_state(bool on)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }
    esp_mqtt_client_enqueue(s_client, TOPIC_LIGHT_STATE, on ? "ON" : "OFF", 0, 1, true, true);
}

void mqtt_manager_publish_sensor_state(float temp, float hum, float pressure)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }

    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f}",
             temp, hum, pressure);

    esp_mqtt_client_enqueue(s_client, TOPIC_SENSOR_STATE, payload, 0, 1, false, true);
}

void mqtt_manager_publish_access_event(access_event_type_t type)
{
    if (s_client == NULL) {
        return;
    }

    char payload[64];
    snprintf(payload, sizeof(payload), "{\"event_type\":\"%s\"}", access_event_type_to_str(type));

    esp_mqtt_client_enqueue(s_client, TOPIC_ACCESS_STATE, payload, 0, 1, false, true);
}

void mqtt_manager_publish_lock_state(bool unlocked)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }
    esp_mqtt_client_enqueue(s_client, TOPIC_LOCK_STATE, unlocked ? "UNLOCKED" : "LOCKED", 0, 1, true, true);
}

void mqtt_manager_publish_fan_state(bool on)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }
    esp_mqtt_client_enqueue(s_client, TOPIC_FAN_STATE, on ? "ON" : "OFF", 0, 1, true, true);
}

void mqtt_manager_publish_presence(bool present)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }
    esp_mqtt_client_enqueue(s_client, TOPIC_PRESENCE, present ? "HOME" : "AWAY", 0, 1, true, true);
}

void mqtt_manager_publish_lux(float lux)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }
    char payload[32];
    snprintf(payload, sizeof(payload), "{\"lux\":%.1f}", lux);
    esp_mqtt_client_enqueue(s_client, TOPIC_LUX, payload, 0, 1, true, true);
}

void mqtt_manager_publish_ml_mode(bool any_auto)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }
    esp_mqtt_client_enqueue(s_client, TOPIC_ML_MODE, any_auto ? "AUTO" : "SHADOW", 0, 1, true, true);
}

void mqtt_manager_publish_ml_stats(float p_light, float p_fan,
                                   float acc_light, float acc_fan,
                                   uint32_t n_light, uint32_t n_fan,
                                   uint32_t total_updates,
                                   bool auto_light, bool auto_fan)
{
    if (s_client == NULL || !s_mqtt_connected) {
        return;
    }
    char payload[224];
    snprintf(payload, sizeof(payload),
             "{\"p_light\":%.2f,\"p_fan\":%.2f,"
             "\"acc_light\":%.0f,\"acc_fan\":%.0f,"
             "\"n_light\":%u,\"n_fan\":%u,"
             "\"updates\":%u,"
             "\"auto_light\":%s,\"auto_fan\":%s}",
             p_light, p_fan, acc_light, acc_fan,
             (unsigned)n_light, (unsigned)n_fan, (unsigned)total_updates,
             auto_light ? "true" : "false", auto_fan ? "true" : "false");
    esp_mqtt_client_enqueue(s_client, TOPIC_ML_STATS, payload, 0, 1, false, true);
}