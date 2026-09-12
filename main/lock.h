#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

// قفل سلونوئیدی Fail-Secure: بدون برق قفل است؛ با اعمال ولتاژ (رله) باز می‌شود.
#define LOCK_RELAY_PIN          GPIO_NUM_21

// سطح فعال رله - با جامپر روی خود ماژول قابل تنظیم است.
// این پروژه رله را روی Low Level Trigger (Active LOW) گذاشته؛
// اگر جامپر را روی High گذاشتی، فقط این خط را به false تغییر بده.
#define LOCK_RELAY_ACTIVE_LOW   false

// تایمر fail-safe سخت‌افزاری داخل خود درایور - عمداً بیشتر از تایمر
// "رسمی" relock در app_state.c (که ۵۰۰۰ میلی‌ثانیه‌ست) تنظیم شده.
// در حالت عادی هیچ‌وقت فرصت شلیک‌شدن پیدا نمی‌کند، چون app_state زودتر
// (در ۵ ثانیه) قفل را می‌بندد و این تایمر را باطل می‌کند. فقط وقتی شلیک
// می‌کند که یک مسیر دیگر (باگ/دیباگ) مستقیم lock_set(true) را صدا زده
// و از app_state عبور نکرده باشد - یعنی یک شبکه‌ی ایمنی فیزیکی، نه بخشی
// از جریان عادی سیستم.
#define LOCK_HW_FAILSAFE_MS      8000

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief راه‌اندازی اولیه‌ی درایور قفل: تنظیم پین رله به‌عنوان خروجی،
 *        اطمینان از قفل‌بودن در بوت (fail-secure)، و ساخت تایمر fail-safe.
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
void lock_init(void);

/**
 * @brief باز/بسته کردن مستقیم قفل (فقط لایه‌ی سخت‌افزار - رله).
 *        منطق "رسمی" relock بعد از چند ثانیه و اطلاع‌رسانی به بقیه‌ی
 *        سیستم (MQTT/LCD) مسئولیت app_state.c است، نه این تابع.
 *        فراخوانی با true فقط تایمر fail-safe داخلی (LOCK_HW_FAILSAFE_MS)
 *        را به‌عنوان شبکه‌ی ایمنی آخر شروع می‌کند.
 */
void lock_set(bool unlocked);

bool lock_is_unlocked(void);

#ifdef __cplusplus
}
#endif