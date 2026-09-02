#pragma once

#include <stdbool.h>

/**
 * @brief راه‌اندازی app_state (ساخت mutex). باید یک‌بار در ابتدای app_main صدا زده شود.
 */
void app_state_init(void);

/**
 * @brief دریافت وضعیت فعلی چراغ (thread-safe).
 */
bool app_state_get_light(void);

/**
 * @brief تنظیم وضعیت چراغ. این تابع نقطه‌ی ورودی مشترک برای همه‌ی منابع
 *        (دکمه‌ی فیزیکی، دکمه‌ی LCD، HTTP) خواهد بود.
 *        فعلاً فقط مقدار داخلی را عوض می‌کند؛ اتصال به LED واقعی در گام بعدی اضافه می‌شود.
 */
void app_state_set_light(bool on);

void app_state_set_lcd_available(bool available);


typedef struct {
    float temperature_c;
    float humidity_percent;
    float pressure_hpa;
    bool  valid;
} sensor_data_t;

void app_state_set_sensor_data(float temp, float hum, float pressure);
sensor_data_t app_state_get_sensor_data(void);