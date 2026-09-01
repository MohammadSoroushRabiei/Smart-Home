#pragma once

#include <stdbool.h>
#include "driver/gpio.h"

#define LED GPIO_NUM_13



void led_init(void);

void led_toggle(gpio_num_t led);

bool led_is_on(void);