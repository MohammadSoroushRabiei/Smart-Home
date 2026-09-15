#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ساخت overlay صفحه‌ی تنظیمات وای‌فای (یک‌بار، بعد از lcd_driver_init).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void wifi_setup_screen_init(void);

/**
 * @brief نمایش صفحه و شروع اسکن شبکه‌های اطراف (در پس‌زمینه، بدون بلاک کردن LVGL).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void wifi_setup_screen_show(void);

/**
 * @brief مخفی‌کردن دستی صفحه (مثلاً دکمه‌ی «انصراف»).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void wifi_setup_screen_hide(void);

#ifdef __cplusplus
}
#endif