#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KEYPAD_PURPOSE_UNLOCK,     // ورود رمز برای باز کردن قفل
    KEYPAD_PURPOSE_SETTINGS,   // ورود رمز برای دسترسی به صفحه‌ی تنظیمات
} keypad_purpose_t;

/**
 * @brief callback ای که بعد از تایید/رد رمز صدا زده می‌شود.
 * @param purpose  همان مقداری که به keypad_screen_show داده شده بود
 * @param success  آیا رمز واردشده صحیح بود؟
 */
typedef void (*keypad_result_cb_t)(keypad_purpose_t purpose, bool success);

/**
 * @brief ساخت overlay کیبورد (یک‌بار، در ابتدای برنامه بعد از lcd_driver_init).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void keypad_screen_init(void);

/**
 * @brief نمایش کیبورد و شروع دریافت رمز.
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void keypad_screen_show(keypad_purpose_t purpose, keypad_result_cb_t on_result);

/**
 * @brief مخفی کردن دستی کیبورد (مثلاً یک دکمه‌ی «انصراف»).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void keypad_screen_hide(void);

#ifdef __cplusplus
}
#endif