#pragma once

#include <stdbool.h>
#include "driver/i2c_master.h"

#define SHARED_I2C_SDA   1
#define SHARED_I2C_SCL   2

/**
 * @brief راه‌اندازی باس I2C مشترک بین Touch (GT911) و BME280.
 *        باید فقط یک‌بار، قبل از touch_driver_init و bme280_init صدا زده شود.
 */
bool i2c_bus_init(void);

i2c_master_bus_handle_t i2c_bus_get_handle(void);