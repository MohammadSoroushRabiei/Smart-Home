#pragma once

#include <stdbool.h>
#include "lvgl.h"

#define TOUCH_PIN_SDA   1
#define TOUCH_PIN_SCL   2
#define TOUCH_PIN_RST   42
#define TOUCH_PIN_INT   41

/**
 * @brief راه‌اندازی I2C، GT911، و ثبت آن به‌عنوان input device برای LVGL.
 * @return true اگر موفق بود، false اگر تاچ در دسترس نبود (سیستم باید بدون آن ادامه یابد).
 */
bool touch_driver_init(lv_display_t *disp);