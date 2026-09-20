#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * صفحه‌ی تمام‌صفحه‌ی حضور و غیاب - از دکمه‌ی Attendance روی صفحه‌ی اصلی
 * باز می‌شود (بدون رمز). شامل:
 *  - QR توکن‌دار /attendance (۱۰ دقیقه؛ با انقضا خودکار تازه می‌شود)
 *  - زمان‌سنج زیر QR
 *  - دکمه‌ی «Enroll New Face»: اول رمز تنظیمات می‌خواهد (کیپد)، بعد همان
 *    صفحه به QR ثبت‌نام /enroll (۳ دقیقه) سوییچ می‌کند.
 *
 * روی lv_layer_top ساخته می‌شود؛ همه‌ی توابع باید بین
 * lcd_driver_lvgl_lock()/lcd_driver_lvgl_unlock() صدا زده شوند.
 */

void attendance_screen_init(void);

/** نمایش صفحه: حالت QR حضور + تولید توکن تازه. */
void attendance_screen_show(void);

/** بستن صفحه و باطل کردن توکن‌های زنده. */
void attendance_screen_hide(void);

/** آیا صفحه همین الان باز است؟ */
bool attendance_screen_is_open(void);

#ifdef __cplusplus
}
#endif
