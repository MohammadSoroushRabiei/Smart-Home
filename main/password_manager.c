#include "password_manager.h"

#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "password_manager";

#define NVS_NAMESPACE     "auth"
#define NVS_KEY_PASSWORD  "password"
#define DEFAULT_PASSWORD  "1234"

static SemaphoreHandle_t s_mutex;

esp_err_t password_manager_init(void)
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

    char buf[PASSWORD_MAX_LEN + 1];
    size_t len = sizeof(buf);
    ret = nvs_get_str(handle, NVS_KEY_PASSWORD, buf, &len);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // اولین اجرا: رمز پیش‌فرض را تنظیم کن
        ESP_LOGW(TAG, "No password set yet, using default: %s (CHANGE THIS!)", DEFAULT_PASSWORD);
        ret = nvs_set_str(handle, NVS_KEY_PASSWORD, DEFAULT_PASSWORD);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read password from NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Password loaded from NVS");
    }

    nvs_close(handle);
    return ret;
}

bool password_manager_verify(const char *input)
{
    if (input == NULL) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "Failed to open NVS for verification: %s", esp_err_to_name(ret));
        return false;
    }

    char stored[PASSWORD_MAX_LEN + 1];
    size_t len = sizeof(stored);
    ret = nvs_get_str(handle, NVS_KEY_PASSWORD, stored, &len);
    nvs_close(handle);

    if (ret != ESP_OK) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "Failed to read password for verification: %s", esp_err_to_name(ret));
        return false;
    }

    bool match = (strcmp(input, stored) == 0);

    xSemaphoreGive(s_mutex);

    return match;
}

esp_err_t password_manager_set(const char *new_password)
{
    if (new_password == NULL) {
        return ESP_ERR_INVALID_ARG;
    }

    size_t len = strlen(new_password);
    if (len < PASSWORD_MIN_LEN || len > PASSWORD_MAX_LEN) {
        ESP_LOGW(TAG, "Rejected password: length %zu out of range [%d, %d]",
                 len, PASSWORD_MIN_LEN, PASSWORD_MAX_LEN);
        return ESP_ERR_INVALID_SIZE;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, NVS_KEY_PASSWORD, new_password);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Password updated successfully");
    } else {
        ESP_LOGE(TAG, "Failed to update password: %s", esp_err_to_name(ret));
    }

    return ret;
}