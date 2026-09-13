#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// طول توکن: ۶ رقم عددی (شبیه کدهای تایید پیامکی) - به اندازه‌ی کافی کوتاه
// برای نمایش راحت در QR، و به اندازه‌ی کافی بزرگ که حدس زدنش در ۳ دقیقه
// (پنجره‌ی اعتبار) عملاً غیرممکن باشد.
#define ENROLL_TOKEN_LEN        6

// مدت اعتبار توکن از لحظه‌ی تولید
#define ENROLL_TOKEN_VALID_MS   (3 * 60 * 1000)

/**
 * @brief راه‌اندازی اولیه‌ی ماژول توکن (ساخت mutex).
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
esp_err_t enroll_token_init(void);

/**
 * @brief تولید یک توکن جدید، جایگزین هر توکن قبلی (در صورت وجود).
 *        باید بعد از تایید موفق رمز در صفحه‌ی Settings صدا زده شود.
 *
 * @param out_token بافر خروجی؛ باید حداقل ENROLL_TOKEN_LEN+1 بایت باشد
 * @param out_size  اندازه‌ی بافر out_token
 */
void enroll_token_generate(char *out_token, size_t out_size);

/**
 * @brief بررسی می‌کند آیا توکن داده‌شده معتبر است (مطابقت + عدم انقضا).
 *        این تابع توکن را باطل نمی‌کند - امکان چند enroll با یک توکن تا
 *        زمان انقضا وجود دارد (تصمیم طراحی عمدی).
 */
bool enroll_token_validate(const char *token);

/**
 * @brief باطل کردن دستی توکن فعلی (مثلاً وقتی کاربر از صفحه‌ی Settings
 *        با دکمه‌ی Back/Cancel خارج می‌شود، نه به‌خاطر انقضا یا شکست enroll).
 */
void enroll_token_invalidate(void);

#ifdef __cplusplus
}
#endif