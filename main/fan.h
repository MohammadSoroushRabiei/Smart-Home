#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

// فن با یک LED واقعی شبیه‌سازی می‌شود (دیگر دستگاه مجازی نیست).
// GPIO47 طبق docs/hardware/pin-mapping.md کاملاً آزاد است؛ GPIO48 عمداً یدک مانده.
#define FAN_GPIO    GPIO_NUM_47

/**
 * @brief راه‌اندازی اولیه‌ی درایور فن: پین به‌عنوان خروجی و در حالت خاموش.
 *        باید یک‌بار در ابتدای app_main کنار led_init/lock_init صدا زده شود.
 */
void fan_init(void);

/**
 * @brief روشن/خاموش کردن فن (سطح GPIO). فقط لایه‌ی سخت‌افزار؛ منطق رسمی
 *        (MQTT/LCD/observer) مسئول app_state.c است — قرینه‌ی lock.c.
 */
void fan_set(bool on);

bool fan_is_on(void);
