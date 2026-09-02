#pragma once

#include <stdbool.h>
#include "driver/i2c_master.h"

typedef struct {
    float temperature_c;
    float humidity_percent;
    float pressure_hpa;
} bme280_data_t;

/**
 * @brief راه‌اندازی BME280 روی باس I2C داده‌شده (chip-id check + خواندن calibration).
 */
bool bme280_init(i2c_master_bus_handle_t bus);

/**
 * @brief یک اندازه‌گیری forced-mode می‌گیرد و مقادیر کمپانزه‌شده را برمی‌گرداند.
 *        بلاک‌کننده (~چند میلی‌ثانیه)، از تسک جداگانه صدا بزنید نه از UI task.
 */
bool bme280_read(bme280_data_t *out);