#pragma once

#include <stdbool.h>

/**
 * @brief راه‌اندازی عامل ML: بارگذاری وزن‌ها (NVS یا پیش‌آموزش)، ثبت
 *        observer روی app_state و شروع تسک تصمیم‌گیر دوره‌ای.
 *        باید بعد از app_state_init و قبل از هر تعامل کاربر صدا زده شود.
 */
void ml_agent_init(void);

/**
 * @brief فرمان دستی از HA (سوییچ ML Autonomy):
 *        true  → هر دو دستگاه خودکار + غیرفعال‌شدن ارتقای خودکار
 *        false → هر دو دستگاه سایه + قفل شدن سایه (ارتقای خودکار معلق)
 */
void ml_agent_set_autonomy(bool auto_mode);

/**
 * @brief آیا هیچ دستگاهی در حالت خودکار است؟ (برای state سوییچ HA)
 */
bool ml_agent_any_auto(void);
