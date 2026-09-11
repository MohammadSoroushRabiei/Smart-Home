#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

// قفل سلونوئیدی Fail-Secure: بدون برق قفل است؛ با اعمال ولتاژ (رله) باز می‌شود.
#define LOCK_RELAY_PIN          GPIO_NUM_21

// سطح فعال رله - با جامپر روی خود ماژول قابل تنظیم است.
// این پروژه رله را روی Low Level Trigger (Active LOW) گذاشته؛
// اگر جامپر را روی High گذاشتی، فقط این خط را به false تغییر بده.
#define LOCK_RELAY_ACTIVE_LOW   false

// بعد از این مدت از باز شدن، خودکار دوباره قفل می‌شود (ایمنی در برابر
// فراموش‌کردن یا قطع شدن نرم‌افزار بعد از باز کردن قفل)
#define LOCK_AUTO_RELOCK_MS     5000

void lock_init(void);

/**
 * @brief باز/بسته کردن مستقیم قفل. فراخوانی با true یک تایمر خودکار
 *        برای قفل مجدد بعد از LOCK_AUTO_RELOCK_MS شروع می‌کند.
 */
void lock_set(bool unlocked);

bool lock_is_unlocked(void);