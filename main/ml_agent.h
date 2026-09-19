#pragma once

#include <stdbool.h>
#include <stdint.h>

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

/**
 * @brief آمار آخرین سیکل عامل برای داشبورد وب — همان چیزی که پاپ‌آپ LCD و
 *        MQTT منتشر می‌کنند: p_* آخرین پیش‌بینی، acc_* دقت پنجره‌ی ارزیابی (٪)
 *        و n_* اندازه‌ی پنجره. هر زمان و از هر تسکی قابل فراخوانی است.
 */
typedef struct {
    bool any_auto;
    float p_light;
    float p_fan;
    float acc_light;
    float acc_fan;
    uint32_t n_light;
    uint32_t n_fan;
} ml_agent_stats_t;

void ml_agent_get_stats(ml_agent_stats_t *out);
