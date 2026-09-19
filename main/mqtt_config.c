#include "mqtt_config.h"

#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "mqtt_config";

#define NVS_NAMESPACE   "mqtt_config"
#define NVS_KEY_HOST    "broker_host"

static SemaphoreHandle_t s_mutex;

esp_err_t mqtt_config_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    // فقط برای اطمینان از باز شدن صحیح namespace در اولین اجرا
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }
    nvs_close(handle);

    ESP_LOGI(TAG, "MQTT config module initialized");
    return ESP_OK;
}

bool mqtt_config_get_host(char *out, size_t out_size)
{
    if (out == NULL || out_size == 0) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        return false;   // هنوز namespace/کلیدی وجود ندارد یعنی چیزی تنظیم نشده
    }

    size_t len = out_size;
    ret = nvs_get_str(handle, NVS_KEY_HOST, out, &len);
    nvs_close(handle);

    xSemaphoreGive(s_mutex);

    return (ret == ESP_OK && out[0] != '\0');
}

esp_err_t mqtt_config_set_host(const char *host)
{
    if (host == NULL || host[0] == '\0' || strlen(host) > MQTT_CONFIG_HOST_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, NVS_KEY_HOST, host);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "MQTT broker host saved: %s", host);
    } else {
        ESP_LOGE(TAG, "Failed to save MQTT broker host: %s", esp_err_to_name(ret));
    }

    return ret;
}