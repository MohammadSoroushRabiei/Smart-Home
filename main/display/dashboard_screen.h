#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/**
 * صفحه‌ی تمام‌صفحه‌ی «Web Dashboard» - از دکمه‌ی هم‌نام روی صفحه‌ی اصلی باز
 * می‌شود. QR آدرس داشبورد وب را نشان می‌دهد و زیر آن فقط آدرس (https://<ip>)
 * می‌آید؛ وقتی WiFi وصل نیست به‌جای آن‌ها پیام آفلاین. روی lv_layer_top
 * ساخته می‌شود؛ همه‌ی توابع باید بین
 * lcd_driver_lvgl_lock()/lcd_driver_lvgl_unlock() صدا زده شوند.
 */

void dashboard_screen_init(void);

/** نمایش صفحه: با آخرین وضعیت WiFi که main فرستاده تازه می‌شود. */
void dashboard_screen_show(void);

/** بستن صفحه. */
void dashboard_screen_hide(void);

/** آیا صفحه همین الان باز است؟ */
bool dashboard_screen_is_open(void);

/**
 * @brief URL داشبورد وب - با هر تغییر وضعیت WiFi از main صدا زده می‌شود
 *        (url == NULL یا رشته‌ی خالی = آفلاین). اگر صفحه باز باشد محتوایش
 *        همان لحظه تازه می‌شود.
 */
void dashboard_screen_set_url(const char *url);

#ifdef __cplusplus
}
#endif
