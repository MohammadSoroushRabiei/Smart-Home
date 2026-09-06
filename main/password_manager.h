#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PASSWORD_MIN_LEN 4
#define PASSWORD_MAX_LEN 8

/**
 * @brief راه‌اندازی ماژول رمز: باز کردن NVS namespace و تنظیم رمز پیش‌فرض
 *        در صورت اولین اجرا (وقتی هنوز رمزی ذخیره نشده).
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
esp_err_t password_manager_init(void);

/**
 * @brief بررسی می‌کند آیا رمز واردشده با رمز ذخیره‌شده مطابقت دارد یا نه.
 */
bool password_manager_verify(const char *input);

/**
 * @brief تنظیم رمز جدید (طول باید بین PASSWORD_MIN_LEN و PASSWORD_MAX_LEN باشد).
 *        در NVS ذخیره می‌شود (پایدار پس از قطع برق).
 */
esp_err_t password_manager_set(const char *new_password);

#ifdef __cplusplus
}
#endif