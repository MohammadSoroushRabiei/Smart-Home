#include "face_db.h"

#include <string.h>
#include "nvs.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "face_db";

#define NVS_NAMESPACE   "face_db"
#define NVS_KEY_LIST    "list"
#define NVS_KEY_COUNT   "count"

static SemaphoreHandle_t s_mutex;
static face_db_entry_t s_entries[FACE_DB_MAX_ENTRIES];
static size_t s_count = 0;

static esp_err_t save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    ret = nvs_set_blob(handle, NVS_KEY_LIST, s_entries, sizeof(face_db_entry_t) * s_count);
    if (ret == ESP_OK) {
        ret = nvs_set_u16(handle, NVS_KEY_COUNT, (uint16_t)s_count);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }

    nvs_close(handle);
    return ret;
}

esp_err_t face_db_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to open NVS namespace: %s", esp_err_to_name(ret));
        return ret;
    }

    uint16_t count = 0;
    ret = nvs_get_u16(handle, NVS_KEY_COUNT, &count);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        s_count = 0;   // اولین اجرا - هنوز چهره‌ای ثبت نشده
        ret = ESP_OK;
    } else if (ret == ESP_OK) {
        size_t required = sizeof(face_db_entry_t) * count;
        ret = nvs_get_blob(handle, NVS_KEY_LIST, s_entries, &required);
        if (ret == ESP_OK) {
            s_count = count;
        } else {
            ESP_LOGW(TAG, "Failed to load face list blob: %s", esp_err_to_name(ret));
            s_count = 0;
        }
    }

    nvs_close(handle);
    ESP_LOGI(TAG, "Loaded %d enrolled face(s) from NVS", (int)s_count);
    return ESP_OK;
}

esp_err_t face_db_add(uint16_t id, const char *name)
{
    if (name == NULL || name[0] == '\0') {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    esp_err_t ret;
    if (s_count >= FACE_DB_MAX_ENTRIES) {
        ESP_LOGW(TAG, "Face DB full (max %d entries)", FACE_DB_MAX_ENTRIES);
        ret = ESP_ERR_NO_MEM;
    } else {
        s_entries[s_count].id = id;
        strncpy(s_entries[s_count].name, name, FACE_DB_NAME_MAX_LEN);
        s_entries[s_count].name[FACE_DB_NAME_MAX_LEN] = '\0';
        s_count++;
        ret = save_to_nvs();
        ESP_LOGI(TAG, "Added face id=%u name=\"%s\"", id, name);
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

esp_err_t face_db_remove(uint16_t id)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    esp_err_t ret = ESP_ERR_NOT_FOUND;
    for (size_t i = 0; i < s_count; i++) {
        if (s_entries[i].id == id) {
            // جابه‌جایی آخرین عضو به این جایگاه - ترتیب لیست اهمیتی ندارد
            s_entries[i] = s_entries[s_count - 1];
            s_count--;
            ret = save_to_nvs();
            ESP_LOGI(TAG, "Removed face id=%u", id);
            break;
        }
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

size_t face_db_get_list(face_db_entry_t *out_array, size_t max_count)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    size_t n = (s_count < max_count) ? s_count : max_count;
    memcpy(out_array, s_entries, n * sizeof(face_db_entry_t));
    xSemaphoreGive(s_mutex);
    return n;
}

bool face_db_get_name(uint16_t id, char *out_buf, size_t out_size)
{
    bool found = false;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (size_t i = 0; i < s_count; i++) {
        if (s_entries[i].id == id) {
            strncpy(out_buf, s_entries[i].name, out_size - 1);
            out_buf[out_size - 1] = '\0';
            found = true;
            break;
        }
    }
    xSemaphoreGive(s_mutex);

    return found;
}

size_t face_db_count_by_name(const char *name)
{
    if (name == NULL) {
        return 0;
    }

    size_t count = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (size_t i = 0; i < s_count; i++) {
        if (strcmp(s_entries[i].name, name) == 0) {
            count++;
        }
    }
    xSemaphoreGive(s_mutex);

    return count;
}

size_t face_db_get_persons(face_db_person_t *out_array, size_t max_count)
{
    size_t n = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    for (size_t i = 0; i < s_count && n < max_count; i++) {
        size_t j;
        for (j = 0; j < n; j++) {
            if (strcmp(out_array[j].name, s_entries[i].name) == 0) {
                out_array[j].sample_count++;
                break;
            }
        }
        if (j == n) {   // نام جدید - به لیست افراد اضافه کن
            strncpy(out_array[n].name, s_entries[i].name, FACE_DB_NAME_MAX_LEN);
            out_array[n].name[FACE_DB_NAME_MAX_LEN] = '\0';
            out_array[n].sample_count = 1;
            n++;
        }
    }
    xSemaphoreGive(s_mutex);

    return n;
}

esp_err_t face_db_remove_by_name(const char *name, uint16_t *removed_ids,
                                 size_t max_ids, size_t *removed_count)
{
    if (name == NULL || removed_count == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    *removed_count = 0;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    bool changed = false;
    for (size_t i = 0; i < s_count; ) {
        if (strcmp(s_entries[i].name, name) == 0) {
            if (removed_ids != NULL && *removed_count < max_ids) {
                removed_ids[(*removed_count)++] = s_entries[i].id;
            }
            // جابه‌جایی آخرین عضو به این جایگاه - ترتیب لیست اهمیتی ندارد
            s_entries[i] = s_entries[s_count - 1];
            s_count--;
            changed = true;
            // بدون i++ - عضو جابه‌جا شده هم باید دوباره بررسی شود
        } else {
            i++;
        }
    }

    esp_err_t ret = ESP_OK;
    if (!changed) {
        ret = ESP_ERR_NOT_FOUND;
    } else {
        ret = save_to_nvs();
        ESP_LOGI(TAG, "Removed %u sample(s) of \"%s\"", (unsigned)*removed_count, name);
    }

    xSemaphoreGive(s_mutex);

    return ret;
}