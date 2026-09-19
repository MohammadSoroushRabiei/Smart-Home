#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PASSWORD_MIN_LEN 4
#define PASSWORD_MAX_LEN 8

/**
 * @brief نوع رمزی که این ماژول مدیریت می‌کند. از این نسخه به بعد، رمز باز
 *        کردن قفل درب (کیبورد صفحه‌ی اصلی) و رمز ورود به منوی تنظیمات دو
 *        رمز کاملاً مستقل هستند و هرکدام جداگانه در منوی تنظیمات قابل
 *        تغییرند.
 */
typedef enum {
    PASSWORD_KIND_LOCK = 0,      // رمز باز کردن قفل درب
    PASSWORD_KIND_SETTINGS = 1,  // رمز ورود به منوی تنظیمات
} password_kind_t;

/**
 * @brief راه‌اندازی ماژول رمز: باز کردن NVS namespace و تنظیم رمز پیش‌فرض
 *        برای هرکدام از دو رمز (قفل / تنظیمات) در صورتی که هنوز ذخیره
 *        نشده باشند.
 *        اگر رمز نسخه‌ی قبلی (تک‌رمزی، قبل از این تغییر) در NVS موجود
 *        باشد، به‌عنوان مقدار اولیه‌ی هر دو رمز جدید استفاده می‌شود - تا
 *        دستگاه‌هایی که از قبل فلش شده‌اند بدون تغییر رمز فعلی‌شان کار
 *        کنند.
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
esp_err_t password_manager_init(void);

/**
 * @brief بررسی می‌کند آیا رمز واردشده با رمز ذخیره‌شده‌ی همان نوع مطابقت دارد یا نه.
 */
bool password_manager_verify(password_kind_t kind, const char *input);

/**
 * @brief تنظیم رمز جدید برای نوع مشخص‌شده (طول باید بین PASSWORD_MIN_LEN
 *        و PASSWORD_MAX_LEN باشد). در NVS ذخیره می‌شود (پایدار پس از قطع برق).
 */
esp_err_t password_manager_set(password_kind_t kind, const char *new_password);

#ifdef __cplusplus
}
#endif