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
 * @brief publish کردن وضعیت واقعی قفل (باز/بسته) به HA - از هر مسیری
 *        که تغییر کند (دستی یا relock خودکار در app_state) صدا زده می‌شود.
 */
void mqtt_manager_publish_lock_state(bool unlocked);

/**
 * @brief انواع رویداد دسترسی که به entity از نوع "event" در HA فرستاده می‌شود.
 *        هر مقدار دقیقاً باید با یکی از رشته‌های "event_types" که در
 *        mqtt_manager.c برای discovery این entity تعریف شده مطابقت داشته باشد.
 */
typedef enum {
    ACCESS_EVENT_GRANTED_FACE,   // ورود موفق با تشخیص چهره
    ACCESS_EVENT_DENIED_FACE,    // ورود ناموفق با تشخیص چهره (چهره‌ی ناشناس)
    ACCESS_EVENT_GRANTED_CODE,   // ورود موفق با رمز از کیبورد LCD
    ACCESS_EVENT_DENIED_CODE,    // ورود ناموفق با رمز از کیبورد LCD
} access_event_type_t;

/**
 * @brief publish کردن یک رویداد دسترسی (لحظه‌ای، stateless) به HA.
 *        بر خلاف بقیه‌ی publish ها، این یک "event" entity است نه sensor -
 *        یعنی هر بار یک رویداد جدید با نوع مشخص ثبت می‌شود، نه یک وضعیت
 *        دائمی ON/OFF.
 */
void mqtt_manager_publish_access_event(access_event_type_t type);

#ifdef __cplusplus
}
#endif