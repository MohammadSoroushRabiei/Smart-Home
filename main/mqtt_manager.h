#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief راه‌اندازی کلاینت MQTT: اتصال به broker، ثبت پیام‌های Discovery برای HA،
 *        و subscribe به تاپیک فرمان چراغ.
 *        باید یک‌بار در ابتدای app_main صدا زده شود (بعد از wifi_init_sta).
 *        این تابع بلاک‌کننده نیست؛ کلاینت esp-mqtt خودش تسک و منطق retry/reconnect دارد،
 *        پس حتی اگر Wi-Fi هنوز وصل نشده باشد، صدا زدن این تابع مشکلی ایجاد نمی‌کند.
 */
void mqtt_manager_init(void);

/**
 * @brief publish کردن وضعیت فعلی چراغ به HA (retained).
 *        باید هر بار که وضعیت چراغ تغییر می‌کند صدا زده شود (از app_state.c).
 */
void mqtt_manager_publish_light_state(bool on);

/**
 * @brief publish کردن مقادیر سنسور به‌صورت یک JSON واحد (دما، رطوبت، فشار).
 */
void mqtt_manager_publish_sensor_state(float temp, float hum, float pressure);

/**
 * @brief publish کردن رویداد دسترسی (باز شدن موفق/ناموفق قفل از طریق چهره یا رمز).
 *        لحظه‌ای است (retained نیست) - صرفاً یک رویداد را در HA لاگ/تریگر می‌کند.
 */
void mqtt_manager_publish_access_event(bool granted);

#ifdef __cplusplus
}
#endif