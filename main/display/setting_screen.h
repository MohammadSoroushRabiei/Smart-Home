#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ساخت overlay صفحه‌ی تنظیمات (یک‌بار، در ابتدای برنامه بعد از lcd_driver_init).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void settings_screen_init(void);

/**
 * @brief نمایش منوی تنظیمات (بعد از تایید موفق رمز Settings).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void settings_screen_show(void);

/**
 * @brief مخفی کردن کامل صفحه‌ی تنظیمات و بازگشت به صفحه‌ی اصلی.
 *        اگر توکن enrollment فعالی وجود داشته باشد، آن را هم باطل می‌کند.
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void settings_screen_hide(void);

#ifdef __cplusplus
}
#endif