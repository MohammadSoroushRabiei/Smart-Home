#pragma once

#include <stdbool.h>
#include "driver/i2c_master.h"

// باس ۰: انحصاریِ تاچ (GT911)
#define TOUCH_I2C_SDA   1
#define TOUCH_I2C_SCL   2

// باس ۱: سنسورهای I2C (فعلاً BME280) - عایق از خطای باس تاچ
#define SENSOR_I2C_SDA  39
#define SENSOR_I2C_SCL  40

/**
 * @brief راه‌اندازی دو باس I2C مستقل:
 *        باس ۰ فقط برای تاچ (GT911) و باس ۱ فقط برای سنسورها (BME280).
 *        باید فقط یک‌بار، قبل از touch_driver_init و bme280_init صدا زده شود.
 */
bool i2c_bus_init(void);

i2c_master_bus_handle_t i2c_bus_touch_handle(void);
i2c_master_bus_handle_t i2c_bus_sensor_handle(void);

/**
 * @brief قفل کردن دسترسی انحصاری به یک باس قبل از یک تراکنش.
 *        باید همیشه با i2c_bus_unlock روی همان باس جفت شود (حتی در مسیرهای خطا/return زودهنگام).
 */
void i2c_bus_lock(i2c_master_bus_handle_t bus);
void i2c_bus_unlock(i2c_master_bus_handle_t bus);
