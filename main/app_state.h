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

/**
 * @brief آیا LCD آماده است؟ ماژول‌های غیر UI (ml_agent، virtual_devices) با
 *        این چک می‌کنند که آیا به‌روزرسانی مستقیم UI برایشان مجاز است.
 */
bool app_state_lcd_available(void);

/**
 * دستگاه‌های قابل کنترل (قفل عمداً در این فهرست نیست - هرگز خودکار نمی‌شود)
 */
typedef enum {
    APP_DEV_LIGHT = 0,
    APP_DEV_FAN    = 1,
} app_device_t;

/**
 * @brief observer تغییر وضعیت دستگاه‌ها. بعد از اعمال کامل تغییر صدا زده
 *        می‌شود؛ from_ml مشخص می‌کند تغییر از عامل ML آمده یا از کاربر.
 *        prev_state برای ویژگی هسترزیس مدل ML لازم است.
 */
typedef void (*app_state_device_cb_t)(app_device_t dev, bool new_state,
                                      bool prev_state, bool from_ml);

/**
 * @brief ثبت observer (فقط یکی نگه داشته می‌شود - ml_agent). باید قبل از
 *        شروع تعامل کاربر صدا زده شود.
 */
void app_state_register_device_cb(app_state_device_cb_t cb);

/**
 * @brief تغییر چراغ از طرف عامل ML (سیگنال آموزش محسوب نمی‌شود).
 */
void app_state_set_light_auto(bool on);

/**
 * @brief وضعیت و کنترل فن (فعلاً مجازی - سخت‌افزار واقعی در گام بعد).
 *        set_fan = مسیر کاربر (آموزش مدل)، set_fan_auto = مسیر مدل.
 */
bool app_state_get_fan(void);
void app_state_set_fan(bool on);
void app_state_set_fan_auto(bool on);


typedef struct {
    float temperature_c;
    float humidity_percent;
    float pressure_hpa;
    bool  valid;
} sensor_data_t;

void app_state_set_sensor_data(float temp, float hum, float pressure);
sensor_data_t app_state_get_sensor_data(void);

bool app_state_get_lock(void);
void app_state_set_lock(bool unlocked);