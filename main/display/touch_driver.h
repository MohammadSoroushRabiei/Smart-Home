#pragma once

#include <stdbool.h>
#include "lvgl.h"
#include "driver/i2c_master.h"

#define TOUCH_PIN_RST   42
#define TOUCH_PIN_INT   41

/**
 * @brief راه‌اندازی GT911 روی باس اختصاصی تاچ، و ثبت آن به‌عنوان input device برای LVGL.
 * @param disp     handle نمایشگر LVGL
 * @param i2c_bus  باس تاچ که با i2c_bus_init ساخته شده (i2c_bus_touch_handle)
 * @return true اگر موفق بود
 */
bool touch_driver_init(lv_display_t *disp, i2c_master_bus_handle_t i2c_bus);