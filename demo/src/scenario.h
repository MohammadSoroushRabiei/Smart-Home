/**
 * @file scenario.h
 * @brief سناریوی خودکار دمو (مثل lv_demo_music) + سیمولاتور سنسورها.
 */
#pragma once

#include <stdbool.h>

/** @brief شروع سنسورساز مجازی (دما/رطوبت/فشار متغیر) - یک‌بار در بوت. */
void demo_sensor_feeder_start(void);

/**
 * @brief اجرای سناریوی خودکار (یک‌بار؛ اجرای دوباره نادیده گرفته می‌شود).
 *        توالی: WiFi/MQTT وصل، فرمان از HA، باز شدن درب با چهره،
 *        ML → AUTO و تصمیم‌های واقعی، صفحه‌ی حضور و غیاب.
 */
void demo_scenario_start(void);
