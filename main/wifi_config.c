#include "wifi_config.h"

#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "wifi_config";

#define NVS_NAMESPACE   "net_config"
#define NVS_KEY_NETS    "known_nets"

static SemaphoreHandle_t s_mutex;

// آرایه‌ی خالی به‌عنوان مقدار پیش‌فرض وقتی هنوز چیزی در NVS ذخیره نشده
static const wifi_known_network_t s_empty_list[WIFI_CONFIG_MAX_NETWORKS] = { 0 };

esp_err_t wifi_config_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_known_network_t tmp[WIFI_CONFIG_MAX_NETWORKS];
    size_t required_size = sizeof(tmp);
    ret = nvs_get_blob(handle, NVS_KEY_NETS, tmp, &required_size);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // اولین اجرا: لیست خالی را ذخیره کن
        ESP_LOGI(TAG, "No known networks yet, initializing empty list");
        ret = nvs_set_blob(handle, NVS_KEY_NETS, s_empty_list, sizeof(s_empty_list));
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read known networks from NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Known networks loaded from NVS");
    }

    nvs_close(handle);
    return ret;
}

int wifi_config_get_all(wifi_known_network_t *out, int max_count)
{
    if (out == NULL || max_count <= 0) {
        return 0;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "Failed to open NVS for read: %s", esp_err_to_name(ret));
        return 0;
    }

    wifi_known_network_t stored[WIFI_CONFIG_MAX_NETWORKS];
    size_t required_size = sizeof(stored);
    ret = nvs_get_blob(handle, NVS_KEY_NETS, stored, &required_size);
    nvs_close(handle);

    xSemaphoreGive(s_mutex);

    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to read known networks: %s", esp_err_to_name(ret));
        return 0;
    }

    int count = 0;
    for (int i = 0; i < WIFI_CONFIG_MAX_NETWORKS && count < max_count; i++) {
        if (stored[i].ssid[0] == '\0') {
            break;   // چون ترتیب MRU همیشه از ابتدا پر می‌شود، به اولین اسلات خالی که رسیدیم یعنی بقیه هم خالی‌اند
        }
        out[count] = stored[i];
        count++;
    }

    return count;
}

esp_err_t wifi_config_promote(const char *ssid, const char *password)
{
    if (ssid == NULL || password == NULL || ssid[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }
    if (strlen(ssid) > WIFI_CONFIG_SSID_MAX_LEN || strlen(password) > WIFI_CONFIG_PASS_MAX_LEN) {
        return ESP_ERR_INVALID_SIZE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "Failed to open NVS for write: %s", esp_err_to_name(ret));
        return ret;
    }

    wifi_known_network_t stored[WIFI_CONFIG_MAX_NETWORKS];
    size_t required_size = sizeof(stored);
    ret = nvs_get_blob(handle, NVS_KEY_NETS, stored, &required_size);
    if (ret != ESP_OK) {
        // اگر خواندن شکست خورد (مثلاً هنوز init نشده)، از یک لیست خالی شروع کن
        memset(stored, 0, sizeof(stored));
    }

    // اگر این SSID از قبل در لیست بود، جایگاهش را آزاد کن (با شیفت‌دادن بقیه به بالا)
    int existing_idx = -1;
    for (int i = 0; i < WIFI_CONFIG_MAX_NETWORKS; i++) {
        if (strcmp(stored[i].ssid, ssid) == 0) {
            existing_idx = i;
            break;
        }
    }
    if (existing_idx >= 0) {
        for (int i = existing_idx; i < WIFI_CONFIG_MAX_NETWORKS - 1; i++) {
            stored[i] = stored[i + 1];
        }
        memset(&stored[WIFI_CONFIG_MAX_NETWORKS - 1], 0, sizeof(stored[0]));
    }

    // بقیه را یک اسلات به پایین شیفت بده تا جا برای ورودی جدید در ایندکس ۰ باز شود
    // (اگر لیست پر بود، قدیمی‌ترین ورودی همین‌جا به‌طور طبیعی از انتهای آرایه بیرون می‌افتد)
    for (int i = WIFI_CONFIG_MAX_NETWORKS - 1; i > 0; i--) {
        stored[i] = stored[i - 1];
    }

    memset(&stored[0], 0, sizeof(stored[0]));
    strncpy(stored[0].ssid, ssid, WIFI_CONFIG_SSID_MAX_LEN);
    strncpy(stored[0].password, password, WIFI_CONFIG_PASS_MAX_LEN);

    ret = nvs_set_blob(handle, NVS_KEY_NETS, stored, sizeof(stored));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    nvs_close(handle);

    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Promoted network '%s' to top of known list", ssid);
    } else {
        ESP_LOGE(TAG, "Failed to save known networks: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t wifi_config_clear_all(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle, NVS_KEY_NETS, s_empty_list, sizeof(s_empty_list));
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Known networks list cleared");
    }

    return ret;
}