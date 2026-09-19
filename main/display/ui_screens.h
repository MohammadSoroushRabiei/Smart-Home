#pragma once

#include <stdbool.h>
#include "wifi_manager.h"
#include "mqtt_manager.h"


void ui_screens_init(void);
void ui_update_wifi_status(wifi_state_t state);
void ui_update_lock_status(bool unlocked);
/**
 * @brief به‌روزرسانی نمایش وضعیت چراغ روی LCD.
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void ui_update_light_status(bool on);
void ui_update_sensor_status(float temp, float hum, float pressure);

/**
 * @brief به‌روزرسانی نمایش وضعیت فن روی LCD (همگام با entity فن در HA).
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void ui_update_fan_status(bool on);

/**
 * @brief کارت وضعیت ML روی صفحه‌ی اصلی: حالت (SHADOW/AUTO)، دقت پنجره‌ی
 *        هر دستگاه و احتمال پیش‌بینی فعلی — همگام با entity های ML در HA.
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void ui_update_ml_status(bool any_auto, float acc_light, uint32_t n_light,
                         float acc_fan, uint32_t n_fan,
                         float p_light, float p_fan);

/**
 * @brief خط محیط مجازی روی کارت ML: حضور + نور محیط — همگام با entity های
 *        Presence/Ambient Light در HA. از تسک شبیه‌ساز (هر ~۵ ثانیه) صدا زده
 *        می‌شود. باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void ui_update_virtual_env(bool presence, float lux);

/**
 * @brief به‌روزرسانی رنگ دکمه‌ی وضعیت MQTT/HA در صفحه‌ی اصلی.
 *        باید بین lcd_driver_lvgl_lock/unlock صدا زده شود.
 */
void ui_update_mqtt_status(mqtt_manager_state_t state);