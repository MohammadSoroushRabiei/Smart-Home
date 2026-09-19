#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    MQTT_MGR_STATE_UNCONFIGURED,   // هنوز هیچ IP/بروکری در NVS ذخیره نشده - خاکستری، تپ = باز کردن تنظیمات
    MQTT_MGR_STATE_DISABLED,       // خاموش - دستی (تپ روی دکمه)، یا نبود شبکه، یا اتمام retry ها - خاکستری
    MQTT_MGR_STATE_CONNECTING,     // در حال تلاش برای اتصال - نارنجی
    MQTT_MGR_STATE_CONNECTED,      // متصل به بروکر - آبی
} mqtt_manager_state_t;

typedef void (*mqtt_manager_state_change_cb_t)(mqtt_manager_state_t new_state);

/**
 * @brief راه‌اندازی ماژول MQTT. اگر آدرس بروکر قبلاً در NVS ذخیره شده باشد،
 *        بلافاصله تلاش برای اتصال شروع می‌شود (مثل WiFi)؛ اگر بعد از چند بار
 *        شکست موفق نشد، به‌صورت خودکار DISABLED (خاکستری) می‌شود. اگر هنوز
 *        هیچ بروکری تنظیم نشده، در حالت UNCONFIGURED می‌ماند.
 *        باید یک‌بار در ابتدای app_main صدا زده شود (بعد از mqtt_config_init
 *        و بعد از ثبت callback با mqtt_manager_register_state_change_cb).
 *        غیربلاک‌کننده.
 */
void mqtt_manager_init(void);

void mqtt_manager_register_state_change_cb(mqtt_manager_state_change_cb_t cb);
mqtt_manager_state_t mqtt_manager_get_state(void);

/**
 * @brief تست اتصال به یک بروکر جدید و ذخیره‌ی آن *فقط در صورت موفقیت*.
 *        بلاک‌کننده (حداکثر ~۵ ثانیه) - همیشه از یک تسک جداگانه صدا بزنید،
 *        هرگز از تسک LVGL.
 *        - اگر همین الان دقیقاً به همین IP متصل باشیم: بدون هیچ قطع/وصل
 *          دوباره‌ای، فوراً true برمی‌گرداند.
 *        - اگر موفق شد: کلاینت فعلی (روی بروکر قبلی، در صورت وجود) متوقف و
 *          آزاد می‌شود، IP جدید در NVS ذخیره می‌شود و کلاینت تست‌شده جایگزین
 *          کلاینت اصلی می‌شود.
 *        - اگر موفق نشد: هیچ تغییری در کلاینت فعلی/NVS داده نمی‌شود.
 * @return true = وصل شد و ذخیره شد، false = وصل نشد و چیزی ذخیره نشد
 */
bool mqtt_manager_connect_and_save(const char *host);

/**
 * @brief فعال‌سازی/تلاش مجدد دستی - وقتی وضعیت DISABLED است (دکمه‌ی MQTT در
 *        حالت خاموش). اگر هنوز هیچ بروکری تنظیم نشده باشد کاری نمی‌کند.
 */
void mqtt_manager_enable(void);

/**
 * @brief خاموش‌کردن دستی و *پایدار* اتصال MQTT (تپ روی دکمه وقتی روشن است).
 *        قبل از قطع، در صورت متصل بودن، "offline" را روی TOPIC_STATUS
 *        به‌صورت بلاک‌کننده منتشر می‌کند. بر خلاف notify_network_lost، این
 *        حالت با وصل‌شدن دوباره‌ی WiFi خودکار برنمی‌گردد - فقط با تپ دوباره.
 */
void mqtt_manager_disable(void);

/**
 * @brief نسخه‌ی بلاک‌کننده (synchronous) برای آماده‌سازی قبل از قطع WiFi:
 *        در صورت متصل بودن، "offline" را روی TOPIC_STATUS منتشر و کلاینت
 *        را متوقف می‌کند. باید از یک تسک پس‌زمینه (هرگز از تسک LVGL) و
 *        *قبل* از فراخوانی wifi_manager_disable() صدا زده شود - تا HA
 *        واقعاً پیام آفلاین را قبل از قطع فیزیکی WiFi دریافت کند.
 */
void mqtt_manager_prepare_for_network_loss(void);

/**
 * @brief باید از main.c هر بار WiFi وضعیتش OFFLINE می‌شود صدا زده شود (از هر
 *        مسیری: تپ روی دکمه، Forget، شکست خودکار). مثل mqtt_manager_disable
 *        عمل می‌کند (offline publish + stop) اما بر خلاف آن، حالت "دستی
 *        خاموش‌شده" را ست نمی‌کند - پس با وصل‌شدن دوباره‌ی WiFi خودکار برمی‌گردد.
 */
void mqtt_manager_notify_network_lost(void);

/**
 * @brief باید از main.c هر بار WiFi به CONNECTED می‌رود صدا زده شود. اگر
 *        کاربر دستی MQTT را خاموش نکرده باشد و بروکری تنظیم شده باشد، تلاش
 *        خودکار برای اتصال به آخرین بروکر شروع می‌شود (مثل WiFi).
 */
void mqtt_manager_notify_network_available(void);

/**
 * @brief publish کردن وضعیت فعلی چراغ به HA (retained).
 */
void mqtt_manager_publish_light_state(bool on);

/**
 * @brief publish کردن مقادیر سنسور به‌صورت یک JSON واحد (دما، رطوبت، فشار).
 */
void mqtt_manager_publish_sensor_state(float temp, float hum, float pressure);

/**
 * @brief publish کردن وضعیت واقعی قفل (باز/بسته) به HA.
 */
void mqtt_manager_publish_lock_state(bool unlocked);

typedef enum {
    ACCESS_EVENT_GRANTED_FACE,
    ACCESS_EVENT_DENIED_FACE,
    ACCESS_EVENT_GRANTED_CODE,
    ACCESS_EVENT_DENIED_CODE,
} access_event_type_t;

void mqtt_manager_publish_access_event(access_event_type_t type);

#ifdef __cplusplus
}
#endif