#include "password_manager.h"

#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "password_manager";

#define NVS_NAMESPACE           "auth"
#define NVS_KEY_PASSWORD_OLD    "password"           
#define NVS_KEY_PASSWORD_LOCK   "pw_lock"
#define NVS_KEY_PASSWORD_SET    "pw_settings"
#define DEFAULT_PASSWORD        "1234"

static SemaphoreHandle_t s_mutex;

static const char *nvs_key_for_kind(password_kind_t kind)
{
    return (kind == PASSWORD_KIND_SETTINGS) ? NVS_KEY_PASSWORD_SET : NVS_KEY_PASSWORD_LOCK;
}

static esp_err_t ensure_default(nvs_handle_t handle, const char *key, const char *fallback_value)
{
    char buf[PASSWORD_MAX_LEN + 1];
    size_t len = sizeof(buf);
    esp_err_t ret = nvs_get_str(handle, key, buf, &len);

    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "No password set for '%s' yet, using default: %s (CHANGE THIS!)", key, fallback_value);
        ret = nvs_set_str(handle, key, fallback_value);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read '%s' from NVS: %s", key, esp_err_to_name(ret));
    }

    return ret;
}

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

    // مهاجرت از نسخه‌ی قبلی تک‌رمزی: اگر رمز قدیمی (کلید "password") در
    // NVS موجود باشد، همان مقدار به‌عنوان پیش‌فرض هر دو رمز جدید (قفل و
    // تنظیمات) استفاده می‌شود - تا دستگاه‌هایی که از قبل با نسخه‌ی
    // قدیمی‌تر فلش شده‌اند، بدون نیاز به وارد کردن رمز جدید ادامه دهند.
    char old_pw[PASSWORD_MAX_LEN + 1];
    size_t old_len = sizeof(old_pw);
    bool have_old = (nvs_get_str(handle, NVS_KEY_PASSWORD_OLD, old_pw, &old_len) == ESP_OK);
    const char *fallback = have_old ? old_pw : DEFAULT_PASSWORD;

    esp_err_t ret_lock = ensure_default(handle, NVS_KEY_PASSWORD_LOCK, fallback);
    esp_err_t ret_set  = ensure_default(handle, NVS_KEY_PASSWORD_SET, fallback);

    nvs_close(handle);

    if (have_old) {
        ESP_LOGI(TAG, "Migrated legacy single password into separate lock/settings passwords");
    }

    return (ret_lock == ESP_OK) ? ret_set : ret_lock;
}

bool password_manager_verify(password_kind_t kind, const char *input)
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
    ret = nvs_get_str(handle, nvs_key_for_kind(kind), stored, &len);
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

esp_err_t password_manager_set(password_kind_t kind, const char *new_password)
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
        ret = nvs_set_str(handle, nvs_key_for_kind(kind), new_password);
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Password (%s) updated successfully",
                 kind == PASSWORD_KIND_SETTINGS ? "settings" : "lock");
    } else {
        ESP_LOGE(TAG, "Failed to update password: %s", esp_err_to_name(ret));
    }

    return ret;
}