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

/**
 * @brief باید هر بار وضعیت وای‌فای تغییر می‌کند صدا زده شود (از همون callback
 *        سراسری wifi_register_state_change_cb در main.c). اگر این صفحه هم‌اکنون
 *        باز و روی زیرصفحه‌ی لیست است، هایلایت شبکه‌ی متصل را زنده به‌روز می‌کند -
 *        بدون نیاز کاربر به خروج و ورود دوباره.
 */
void wifi_setup_screen_notify_state_change(void);

#ifdef __cplusplus
}
#endif