#pragma once

#include "driver/gpio.h"
#include <stdbool.h>

#define BTN GPIO_NUM_4


void btn_init(void);

bool btn_is_pressed(gpio_num_t btn);