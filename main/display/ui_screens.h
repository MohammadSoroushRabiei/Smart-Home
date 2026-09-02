#pragma once

#include <stdbool.h>
#include "wifi_manager.h"

void ui_screens_init(void);
void ui_update_wifi_status(wifi_state_t state);

/**
 * @brief به‌روزرسانی نمایش وضعیت چراغ روی LCD.
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void ui_update_light_status(bool on);
void ui_update_sensor_status(float temp, float hum, float pressure);
void ui_update_wifi_ip(const char *ip_str);