#pragma once

#include <stdbool.h>
#include <stdint.h>

// وزن‌های پیش‌آموزش (تولید ml/train.py) - اینجا include می‌شود تا ترتیب
// include در فایل‌های دیگر مهم نباشد و ML_N_FEATURES همیشه تعریف باشد
#include "ml_model_weights.h"

// باید با ml_model_weights.h (تولیدشده توسط ml/train.py) یکی باشد
#if !defined(ML_N_FEATURES)
#error "ml_model.h requires ml_model_weights.h to be included first"
#endif

/**
 * @brief یک مدل رگرسیون لجستیک سبک + شمارنده‌ی به‌روزرسانی‌های آنلاین.
 *        وزن‌های اولیه از پیش‌آموزش (ml_model_weights.h) می‌آیند و با
 *        یادگیری آنلاین روی میکرو تغییر می‌کنند.
 */
typedef struct {
    float w[ML_N_FEATURES];
    uint32_t updates;   // تعداد نمونه‌هایی که با SGD دیده است
} ml_linear_t;

/**
 * @brief احتمال خروجی 1 (سیگموئید) برای بردار ویژگی x.
 */
float ml_predict(const ml_linear_t *m, const float x[ML_N_FEATURES]);

/**
 * @brief یک گام SGD روی یک نمونه (x, y) با نرخ یادگیری lr.
 *        وزن‌ها کلمپ می‌شوند تا با نویز آنلاین منفجر نشوند.
 */
void ml_train_sample(ml_linear_t *m, const float x[ML_N_FEATURES], int y, float lr);

/**
 * @brief بارگذاری وزن‌های آموزش‌دیده از NVS (اگر با نسخه‌ی مدل فعلی سازگار
 *        باشد). اگر نداشت/ناسازگار بود false برمی‌گرداند و فراخواننده باید
 *        مدل را با وزن‌های پیش‌آموزش (ML_LIGHT_W / ML_FAN_W) seed کند.
 *
 * @param cfg_flags بیت 0: چراغ در حالت خودکار | بیت 1: فن خودکار |
 *                  بیت 2: قفل دستی سایه (سوییچ HA روی shadow)
 */
bool ml_persistence_load(ml_linear_t *light, ml_linear_t *fan, uint8_t *cfg_flags);

/**
 * @brief ذخیره‌ی وزن‌ها و تنظیمات در NVS.
 */
bool ml_persistence_save(const ml_linear_t *light, const ml_linear_t *fan, uint8_t cfg_flags);
