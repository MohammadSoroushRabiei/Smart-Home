#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// طول توکن: ۶ رقم عددی (شبیه کدهای تایید پیامکی) - به اندازه‌ی کافی کوتاه
// برای نمایش راحت در QR، و به اندازه‌ی کافی بزرگ که حدس زدنش در پنجره‌ی
// اعتبار عملاً غیرممکن باشد.
#define ENROLL_TOKEN_LEN        6

// مدت اعتبار توکن از لحظه‌ی تولید - جدا برای هر منظور. توکن حضور و غیاب
// طولانی‌تر است چون در شلوغی صبح چند نفر پشت‌سرهم اسکن می‌کنند.
#define ENROLL_TOKEN_VALID_MS   (3 * 60 * 1000)
#define ATTEND_TOKEN_VALID_MS   (10 * 60 * 1000)

// هر توکن به یک منظور وصل است؛ دو منظور مستقل از هم کار می‌کنند تا بتوان
// مثلاً QR حضور و QR ثبت‌نام را همزمان زنده نگه داشت.
typedef enum {
    ENROLL_TOKEN_PURPOSE_ENROLL     = 0,   // لینک /enroll - ثبت چهره‌ی جدید
    ENROLL_TOKEN_PURPOSE_ATTENDANCE = 1,   // لینک /attendance - ثبت حضور و غیاب
} enroll_token_purpose_t;

/**
 * @brief راه‌اندازی اولیه‌ی ماژول توکن (ساخت mutex).
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
esp_err_t enroll_token_init(void);

/**
 * @brief تولید یک توکن جدید برای منظور مشخص. توکن فعلیِ همان منظور به
 *        «اسلات قبلی» می‌رود و تا پایان اعتبار خودش همچنان معتبر می‌ماند
 *        (گریس: کسی که QR قبلی را اسکن کرده و هنوز submit نکرده قطع نشود).
 *
 * @param purpose   منظور توکن (enroll یا attendance)
 * @param out_token بافر خروجی؛ باید حداقل ENROLL_TOKEN_LEN+1 بایت باشد
 * @param out_size  اندازه‌ی بافر out_token
 */
void enroll_token_generate(enroll_token_purpose_t purpose, char *out_token, size_t out_size);

/**
 * @brief بررسی می‌کند آیا توکن داده‌شده برای منظور موردنظر معتبر است
 *        (مطابقت با اسلات جاری یا قبلی + عدم انقضا).
 *        این تابع توکن را باطل نمی‌کند - امکان چندبار استفاده تا زمان
 *        انقضا وجود دارد (تصمیم طراحی عمدی: چند نفر با یک QR).
 */
bool enroll_token_validate(enroll_token_purpose_t purpose, const char *token);

/**
 * @brief باطل کردن دستی توکن‌های یک منظور (اسلات جاری و قبلی) - مثلاً وقتی
 *        کاربر از صفحه‌ی QR با دکمه‌ی Back/Cancel خارج می‌شود.
 */
void enroll_token_invalidate(enroll_token_purpose_t purpose);

#ifdef __cplusplus
}
#endif
