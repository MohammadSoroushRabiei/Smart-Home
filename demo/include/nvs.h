/**
 * @file nvs.h
 * @brief شیم PC برای NVS - یک حافظه‌ی key/value در حافظه که ml_model با آن
 *        وزن‌های یادگرفته را بین اجراها ذخیره/بازیابی می‌کند.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef uint32_t nvs_handle_t;

#define NVS_READONLY   0
#define NVS_READWRITE  1

esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *out);
void nvs_close(nvs_handle_t h);
esp_err_t nvs_commit(nvs_handle_t h);
esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v);
esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *v);
esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v);
esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v);
esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t len);
esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len);
esp_err_t nvs_erase_all(nvs_handle_t h);
