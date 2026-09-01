#pragma once

#include "wifi_manager.h"

/**
 * @brief ساخت صفحه‌ی اصلی UI (شامل placeholder برای وضعیت Wi-Fi).
 *        باید بعد از lcd_driver_init() و touch_driver_init() صدا زده شود.
 */
void ui_screens_init(void);

/**
 * @brief به‌روزرسانی نمایش وضعیت Wi-Fi روی صفحه‌ی اصلی.
 *        Thread-safe نیست به‌تنهایی؛ باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void ui_update_wifi_status(wifi_state_t state);