#pragma once

#include "mqtt_manager.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief ساخت overlay صفحه‌ی تنظیمات MQTT (یک‌بار، بعد از lcd_driver_init).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void mqtt_setup_screen_init(void);

/**
 * @brief نمایش صفحه‌ی تنظیمات؛ فیلد IP را با مقدار فعلی (در صورت وجود) پر می‌کند.
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void mqtt_setup_screen_show(void);

/**
 * @brief مخفی‌کردن دستی صفحه (دکمه‌ی Back).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void mqtt_setup_screen_hide(void);

/**
 * @brief باید هر بار وضعیت اتصال MQTT تغییر می‌کند صدا زده شود (از همون callback
 *        سراسری mqtt_manager_register_state_change_cb در main.c). اگر این صفحه
 *        هم‌اکنون باز است، لیبل وضعیت را زنده به‌روز می‌کند.
 */
void mqtt_setup_screen_notify_state_change(mqtt_manager_state_t state);

#ifdef __cplusplus
}
#endif