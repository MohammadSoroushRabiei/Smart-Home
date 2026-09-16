#include "wifi_manager.h"
#include "wifi_config.h"

#include <string.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"


#define WIFI_RETRIES_PER_NETWORK   3   // در حالت پیمایش لیست: قبل از رفتن سراغ شبکه‌ی بعدی
#define WIFI_MANUAL_RETRIES        3   // در حالت اتصال دستی (connect_and_save)
#define WIFI_MANUAL_TIMEOUT_MS     15000

static const char *TAG = "WIFI";

static wifi_state_t wifi_state = WIFI_STATE_OFFLINE;
static wifi_state_change_cb_t s_state_change_cb = NULL;

static char s_ip_str[16] = "";   // "255.255.255.255" حداکثر ۱۵ کاراکتر + نال
static char s_connected_ssid[33] = "";

static bool s_radio_started = false;
static bool s_user_disabled = false;

// اگر true باشد، یعنی خودمان عمداً esp_wifi_disconnect() را صدا زده‌ایم
// (قبل از اسکن یا اتصال دستی) - رویداد DISCONNECTED بعدی باید نادیده گرفته شود
// و وارد منطق retry نشود.
static bool s_expect_disconnect = false;

// --- حالت پیمایش لیست MRU (برای wifi_manager_enable) ---
static wifi_known_network_t s_try_list[WIFI_CONFIG_MAX_NETWORKS];
static int s_try_count = 0;
static int s_trying_index = 0;
static uint8_t s_retry_count = 0;

// --- حالت اتصال دستی (برای wifi_manager_connect_and_save) ---
static bool s_manual_mode = false;
static char s_manual_ssid[33];
static char s_manual_pass[65];
static uint8_t s_manual_retry_count = 0;
static bool s_manual_connect_success = false;
static SemaphoreHandle_t s_manual_connect_sem = NULL;


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

const char *wifi_get_connected_ssid(void)
{
    return s_connected_ssid;
}

void wifi_register_state_change_cb(wifi_state_change_cb_t cb)
{
    s_state_change_cb = cb;
}

static const char *wifi_state_to_string(wifi_state_t state)
{
    switch (state) {
        case WIFI_STATE_OFFLINE:    return "OFFLINE";
        case WIFI_STATE_CONNECTING: return "CONNECTING";
        case WIFI_STATE_CONNECTED:  return "CONNECTED";
        default:                    return "ERROR";
    }
}

static void wifi_set_state(wifi_state_t state)
{
    if (wifi_state == state) {
        return;
    }
    wifi_state = state;
    ESP_LOGI(TAG, "Status: %s", wifi_state_to_string(state));
    if (s_state_change_cb != NULL) {
        s_state_change_cb(state);
    }
}

static void connect_to_index(int idx)
{
    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, s_try_list[idx].ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, s_try_list[idx].password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    s_retry_count = 0;
    wifi_set_state(WIFI_STATE_CONNECTING);
    esp_wifi_connect();
}

static void event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        // ⚠️ برخلاف قبل، دیگر اینجا اتصال خودکار شروع نمی‌شود - این کار وظیفه‌ی
        // صریح wifi_manager_enable/connect_and_save است تا سیستم واقعاً از ابتدا
        // آفلاین بوت شود.
        return;
    }

    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *event = (wifi_event_sta_disconnected_t *)event_data;
        ESP_LOGI(TAG, "SSID: %s / REASON_CODE: %d", event->ssid, event->reason);

        if (s_expect_disconnect) {
            // این قطعی خودمان عمداً درخواستش کرده بودیم (قبل از اسکن/اتصال دستی) -
            // نباید وارد منطق retry بشود.
            s_expect_disconnect = false;
            return;
        }

        s_ip_str[0] = '\0';

        if (s_user_disabled) {
            return;   // کاربر دستی خاموش کرده - هیچ تلاش خودکاری انجام نده
        }

        if (s_manual_mode) {
            s_manual_retry_count++;
            if (s_manual_retry_count < WIFI_MANUAL_RETRIES) {
                wifi_set_state(WIFI_STATE_CONNECTING);
                esp_wifi_connect();
            } else {
                // به‌جای معطل‌ماندن تا پایان کامل timeout، همین الان شکست را اعلام کن
                s_manual_connect_success = false;
                xSemaphoreGive(s_manual_connect_sem);
            }
            return;
        }

        // حالت پیمایش لیست MRU
        s_retry_count++;
        if (s_retry_count < WIFI_RETRIES_PER_NETWORK) {
            wifi_set_state(WIFI_STATE_CONNECTING);
            esp_wifi_connect();
        } else {
            s_trying_index++;
            if (s_trying_index < s_try_count) {
                ESP_LOGI(TAG, "Network failed, trying next known network (%d/%d)",
                         s_trying_index + 1, s_try_count);
                connect_to_index(s_trying_index);
            } else {
                ESP_LOGW(TAG, "All known networks failed, staying offline");
                wifi_set_state(WIFI_STATE_OFFLINE);
            }
        }
    }

    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));

        if (s_manual_mode) {
            strncpy(s_connected_ssid, s_manual_ssid, sizeof(s_connected_ssid) - 1);
            s_manual_connect_success = true;
            wifi_set_state(WIFI_STATE_CONNECTED);
            xSemaphoreGive(s_manual_connect_sem);
        } else {
            // اتصال از طریق لیست MRU موفق شد - این شبکه را (دوباره) بالای لیست ببر
            // تا اگر از ایندکس ۱ یا ۲ وصل شده باشیم، رتبه‌اش به‌روز شود.
            if (s_trying_index < s_try_count) {
                strncpy(s_connected_ssid, s_try_list[s_trying_index].ssid, sizeof(s_connected_ssid) - 1);
                wifi_config_promote(s_try_list[s_trying_index].ssid, s_try_list[s_trying_index].password);
            }
            wifi_set_state(WIFI_STATE_CONNECTED);
        }
    }
}

void wifi_manager_init_radio(void)
{
    s_manual_connect_sem = xSemaphoreCreateBinary();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // ⚠️ عمداً esp_wifi_start() یا esp_wifi_connect() اینجا صدا زده نمی‌شود -
    // سیستم باید واقعاً آفلاین بوت شود؛ روشن‌کردن رادیو وظیفه‌ی wifi_manager_enable
    // یا wifi_manager_scan است.

    ESP_LOGI(TAG, "WiFi radio subsystem initialized (offline)");
}

void wifi_manager_enable(void)
{
    if (wifi_is_connected()) {
        return;   // از قبل متصلیم، کاری لازم نیست
    }

    s_user_disabled = false;

    if (!s_radio_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_radio_started = true;
    }

    s_try_count = wifi_config_get_all(s_try_list, WIFI_CONFIG_MAX_NETWORKS);
    s_trying_index = 0;

    if (s_try_count == 0) {
        ESP_LOGI(TAG, "No known networks saved, staying offline");
        wifi_set_state(WIFI_STATE_OFFLINE);
        return;
    }

    ESP_LOGI(TAG, "Trying known networks (%d saved), starting with most recent", s_try_count);
    connect_to_index(0);
}

void wifi_manager_disable(void)
{
    s_user_disabled = true;

    if (s_radio_started) {
        s_expect_disconnect = true;
        esp_wifi_disconnect();
        esp_wifi_stop();
        s_radio_started = false;
    }

    s_ip_str[0] = '\0';
    wifi_set_state(WIFI_STATE_OFFLINE);
    ESP_LOGI(TAG, "WiFi disabled by user");
}

int wifi_manager_scan(wifi_scan_result_t *out, int max_results)
{
    if (out == NULL || max_results <= 0) {
        return 0;
    }

    bool was_connected = wifi_is_connected();
    if (was_connected) {
        s_expect_disconnect = true;
        esp_wifi_disconnect();
    }
    if (!s_radio_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_radio_started = true;
    }

    wifi_scan_config_t scan_config = { .show_hidden = false };
    esp_err_t err = esp_wifi_scan_start(&scan_config, true);   // بلاک‌کننده
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Scan failed: %s", esp_err_to_name(err));
        if (was_connected) {
            esp_wifi_connect();
        }
        return 0;
    }

    uint16_t ap_count = 0;
    esp_wifi_scan_get_ap_num(&ap_count);
    if (ap_count == 0) {
        if (was_connected) {
            esp_wifi_connect();
        }
        return 0;
    }

    wifi_ap_record_t *records = malloc(sizeof(wifi_ap_record_t) * ap_count);
    if (records == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for scan results");
        if (was_connected) {
            esp_wifi_connect();
        }
        return 0;
    }
    esp_wifi_scan_get_ap_records(&ap_count, records);

    // مرتب‌سازی نزولی بر اساس RSSI (insertion sort - تعداد AP معمولاً کمه)
    for (int i = 1; i < ap_count; i++) {
        wifi_ap_record_t key = records[i];
        int j = i - 1;
        while (j >= 0 && records[j].rssi < key.rssi) {
            records[j + 1] = records[j];
            j--;
        }
        records[j + 1] = key;
    }

    int count = 0;
    for (int i = 0; i < ap_count && count < max_results; i++) {
        if (records[i].ssid[0] == '\0') {
            continue;   // شبکه‌ی مخفی، رد کن
        }
        bool dup = false;
        for (int k = 0; k < count; k++) {
            if (strcmp(out[k].ssid, (char *)records[i].ssid) == 0) {
                dup = true;
                break;
            }
        }
        if (dup) {
            continue;
        }
        strncpy(out[count].ssid, (char *)records[i].ssid, sizeof(out[count].ssid) - 1);
        out[count].rssi = records[i].rssi;
        out[count].authmode = records[i].authmode;
        count++;
    }

    free(records);

    if (was_connected) {
        esp_wifi_connect();   // اتصال قبلی را دوباره برقرار کن
    }

    return count;
}

esp_err_t wifi_manager_connect_and_save(const char *ssid, const char *password)
{
    if (ssid == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_manual_ssid, ssid, sizeof(s_manual_ssid) - 1);
    s_manual_ssid[sizeof(s_manual_ssid) - 1] = '\0';
    strncpy(s_manual_pass, password ? password : "", sizeof(s_manual_pass) - 1);
    s_manual_pass[sizeof(s_manual_pass) - 1] = '\0';

    s_manual_retry_count = 0;
    s_manual_connect_success = false;
    s_manual_mode = true;
    s_user_disabled = false;

    if (!s_radio_started) {
        ESP_ERROR_CHECK(esp_wifi_start());
        s_radio_started = true;
    } else if (wifi_is_connected()) {
        s_expect_disconnect = true;
        esp_wifi_disconnect();
    }

    wifi_config_t wifi_config = { 0 };
    strncpy((char *)wifi_config.sta.ssid, s_manual_ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, s_manual_pass, sizeof(wifi_config.sta.password) - 1);
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    xSemaphoreTake(s_manual_connect_sem, 0);   // هر سیگنال باقی‌مانده از قبل را خالی کن
    wifi_set_state(WIFI_STATE_CONNECTING);
    esp_wifi_connect();

    BaseType_t got_signal = xSemaphoreTake(s_manual_connect_sem, pdMS_TO_TICKS(WIFI_MANUAL_TIMEOUT_MS));
    s_manual_mode = false;

    if (got_signal == pdTRUE && s_manual_connect_success) {
        wifi_config_promote(s_manual_ssid, s_manual_pass);
        return ESP_OK;
    }

    wifi_set_state(WIFI_STATE_OFFLINE);
    return ESP_FAIL;
}