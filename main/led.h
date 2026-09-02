#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#define LED GPIO_NUM_13



void led_init(void);

void led_toggle(gpio_num_t led);

/**
 * @brief تنظیم مستقیم وضعیت LED به یک مقدار مشخص (بدون toggle).
 */
void led_set(gpio_num_t led, bool on);

bool led_is_on(void);