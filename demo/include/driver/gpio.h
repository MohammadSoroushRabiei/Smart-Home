/**
 * @file gpio.h
 * @brief شیم PC برای درایور GPIO - led.c و lock.c واقعی با همین هدر کامپایل
 *        می‌شوند و هر تغییر پین به‌جای سخت‌افزار لاگ می‌شود.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

typedef enum {
    GPIO_NUM_4 = 4,
    GPIO_NUM_13 = 13,
    GPIO_NUM_21 = 21,
} gpio_num_t;

typedef struct {
    int pin_bit_mask;
    int mode;            // GPIO_MODE_OUTPUT
    int pull_up_en;
    int pull_down_en;
    int intr_type;
} gpio_config_t;

#define GPIO_MODE_OUTPUT      2
#define GPIO_PULLUP_DISABLE   0
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE     0

esp_err_t gpio_config(const gpio_config_t *cfg);
int gpio_set_level(gpio_num_t gpio, int level);
esp_err_t gpio_set_direction(gpio_num_t gpio, int mode);
void gpio_reset_pin(gpio_num_t gpio);
