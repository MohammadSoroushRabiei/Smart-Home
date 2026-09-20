#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// آدرس پایه‌ی سرور لوکال (بدون مسیر) - دستگاه خودش /api/event و /health
// را به انتهایش اضافه می‌کند. http (LAN + secret) یا https هر دو مجازند.
#define ATTENDANCE_SERVER_URL_MAX_LEN  128
#define ATTENDANCE_SECRET_MAX_LEN      64
// باید هم‌اندازه‌ی بافر نام face_db باشد (char name[24])
#define ATTENDANCE_NAME_MAX            24

// انواع رویدادی که به سرور لوکال فرستاده می‌شود
typedef enum {
    ATT_EVENT_ATTENDANCE_IN  = 0,   // ثبت ورود
    ATT_EVENT_ATTENDANCE_OUT = 1,   // ثبت خروج
    ATT_EVENT_DOOR_FACE      = 2,   // باز شدن درب با چهره
    ATT_EVENT_DOOR_CODE      = 3,   // باز شدن درب با رمز
    ATT_EVENT_TEST           = 4,   // فقط تست اتصال - ذخیره نمی‌شود
} attendance_event_t;

/**
 * @brief راه‌اندازی ماژول: کانفیگ NVS، بازیابی صف معوق و ساخت تسک ارسال.
 *        باید یک‌بار در app_main بعد از راه‌اندازی NVS صدا زده شود.
 */
esp_err_t attendance_init(void);

/**
 * @brief آیا ارسال به سرور فعال است؟ (کانفیگ URL + enabled)
 */
bool attendance_is_enabled(void);

/**
 * @brief خواندن کانفیگ فعلی. هر پارامتر خروجی می‌تواند NULL باشد.
 * @return true اگر URL قبلاً تنظیم شده باشد (کانفیگ شده)
 */
bool attendance_get_config(char *url, size_t url_size,
                           char *secret, size_t secret_size,
                           bool *enabled);

/**
 * @brief ذخیره‌ی کانفیگ. اگر secret == NULL یا خالی باشد، secret قبلی حفظ
 *        می‌شود. url آدرسِ پایه است (http://IP:port) و در حالت enabled باید
 *        با http:// یا https:// شروع شود.
 */
esp_err_t attendance_set_config(const char *url, const char *secret, bool enabled);

/**
 * @brief تعداد رکوردهای معوق در صف NVS (برای نمایش در داشبورد)
 */
int attendance_queue_count(void);

/**
 * @brief ثبت رویداد حضور (ورود/خروج). ساعت محلی خودش گرفته می‌شود (نیازمند
 *        sync شدن NTP)، رکورد در صف NVS می‌رود و تسک ارسال بیدار می‌شود.
 *        ضدانتشار: همان شخص + همان نوع در ۶۰ ثانیه رکورد جدید نمی‌سازد.
 *
 * @param was_duplicate خروجی اختیاری: true یعنی به‌خاطر ضدانتشار رکورد
 *                      جدیدی ثبت نشد (خود عملیات خطا نیست)
 * @return ESP_OK یا ESP_ERR_INVALID_STATE (ساعت همگام نیست) یا
 *         ESP_ERR_NOT_SUPPORTED (فیچر غیرفعال) یا خطای ذخیره‌سازی
 */
esp_err_t attendance_record(const char *name, uint16_t person_id,
                            attendance_event_t event, float similarity,
                            bool *was_duplicate);

/**
 * @brief گزارش رویدادهای درب (چهره/رمز) به سرور - بدون گیت ساعت و بدون
 *        ضدانتشار (سرور در بدترین حالت زمان دریافت خودش را می‌زند).
 *        غیربلاک‌کننده: فقط در صف می‌نویسد و برمی‌گردد؛ از هندلرهای HTTP
 *        قابل فراخوانی است.
 */
esp_err_t attendance_report_event(attendance_event_t event, const char *name,
                                  uint16_t person_id, float similarity);

/**
 * @brief تست اتصال به سرور (GET {url}/health با secret). بلاک‌کننده تا
 *        ~۱۰ ثانیه - فقط از تسک با استک کافی صدا زده شود (نه httpd و نه
 *        face_worker).
 */
esp_err_t attendance_test_server(void);

#ifdef __cplusplus
}
#endif
