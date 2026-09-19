#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MQTT_CONFIG_HOST_MAX_LEN   64   // IP یا hostname (بدون پورت)

/**
 * @brief راه‌اندازی ماژول: باز کردن NVS namespace و ساخت mutex.
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
esp_err_t mqtt_config_init(void);

/**
 * @brief دریافت آدرس بروکر ذخیره‌شده در NVS.
 * @param out       بافر خروجی
 * @param out_size  اندازه‌ی بافر
 * @return true اگر بروکری قبلاً تنظیم شده (out پر می‌شود)، false اگر هنوز چیزی ذخیره نشده
 */
bool mqtt_config_get_host(char *out, size_t out_size);

/**
 * @brief ذخیره‌ی آدرس بروکر جدید در NVS (از صفحه‌ی تنظیمات MQTT صدا زده می‌شود).
 */
esp_err_t mqtt_config_set_host(const char *host);

#ifdef __cplusplus
}
#endif