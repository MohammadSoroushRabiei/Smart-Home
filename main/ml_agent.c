#include "ml_agent.h"

#include <math.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "app_state.h"
#include "ml_model.h"
#include "ml_model_weights.h"
#include "mqtt_manager.h"
#include "time_sync.h"
#include "virtual_devices.h"

static const char *TAG = "ml_agent";

#define ML_TASK_PERIOD_MS   30000    // دوره‌ی تصمیم‌گیری
#define ML_TASK_STACK       4096
#define ML_TASK_PRIORITY    3

// آستانه‌های ارتقای سایه → خودکار
#define ML_WINDOW_SIZE      20       // پنجره‌ی ارزیابی روی آخرین تصمیم‌های کاربر
#define ML_AUTO_MIN_N       15       // حداقل نمونه‌ی لازم برای ارتقا
#define ML_AUTO_ACC_PCT     85.0f    // حداقل دقت پنجره برای ارتقا

// در حالت خودکار فقط با اطمینان بالا عمل می‌شود (هسترزیس تصمیم)
#define ML_ACT_ON_P         0.75f
#define ML_ACT_OFF_P        0.25f

#define ML_ONLINE_LR        0.08f

// بیت‌های cfg در NVS
#define ML_CFG_AUTO_LIGHT   0x01
#define ML_CFG_AUTO_FAN     0x02
#define ML_CFG_SHADOW_LOCK  0x04

typedef enum {
    ML_MODE_SHADOW = 0,   // فقط پیش‌بینی و ثبت - بدون هیچ عملی
    ML_MODE_AUTO   = 1,   // خودش عمل می‌کند؛ دست کاربر مقدم است
} ml_mode_t;

typedef struct {
    ml_linear_t model;
    ml_mode_t mode;
    uint32_t window_bits;    // بیت 0 = جدیدترین؛ 1 = پیش‌بینی با اقدام کاربر منطبق بود
    uint32_t window_n;       // تعداد تصمیم‌های داخل پنجره
    uint32_t window_hits;
} ml_dev_t;

static SemaphoreHandle_t s_mutex = NULL;
static ml_dev_t s_light;
static ml_dev_t s_fan;
static volatile bool s_dirty = false;      // نیاز به ذخیره در NVS
static bool s_shadow_lock = false;         // کاربر با سوییچ HA سایه را قفل کرده

// ---------------------------------------------------------------------
// ابزارهای داخلی
// ---------------------------------------------------------------------

static float acc_pct(const ml_dev_t *d)
{
    if (d->window_n == 0) {
        return 0.0f;
    }
    return 100.0f * (float)d->window_hits / (float)d->window_n;
}

static void window_push(ml_dev_t *d, bool correct)
{
    if (d->window_n == ML_WINDOW_SIZE) {
        if ((d->window_bits >> (ML_WINDOW_SIZE - 1)) & 1u) {
            d->window_hits--;
        }
        d->window_bits &= ~(1u << (ML_WINDOW_SIZE - 1));
    } else {
        d->window_n++;
    }
    d->window_bits <<= 1;
    if (correct) {
        d->window_bits |= 1u;
        d->window_hits++;
    }
}

/**
 * @brief ویژگی‌های مشترک (بدون بیت 8 که وضعیت فعلی همان دستگاه است) -
 *        قرینه‌ی_exactload در ml/train.py:
 *        0:bias 1:sin(hour) 2:cos(hour) 3:weekend 4:presence
 *        5:lux_n 6:temp_n 7:hum_n
 */
static bool build_features_base(float x[ML_N_FEATURES])
{
    if (!time_sync_is_ready()) {
        return false;
    }
    time_t now = time(NULL);
    struct tm tm_now;
    localtime_r(&now, &tm_now);
    float hour = (float)tm_now.tm_hour +
                 (float)tm_now.tm_min / 60.0f +
                 (float)tm_now.tm_sec / 3600.0f;

    bool presence;
    float lux;
    if (!virtual_devices_get_env(&presence, &lux)) {
        presence = true;
        lux = 5.0f;   // تا آماده‌شدن شبیه‌ساز: مقدار اولیه‌ی محافظه‌کارانه
    }

    sensor_data_t s = app_state_get_sensor_data();
    float temp = s.valid ? s.temperature_c : 24.0f;   // بدون BME280 هم مدل کار می‌کند
    float hum  = s.valid ? s.humidity_percent : 40.0f;

    float lux_n = lux > 1000.0f ? 1.0f : lux / 1000.0f;
    float temp_n = (temp - 15.0f) / 15.0f;
    if (temp_n < 0.0f) temp_n = 0.0f;
    if (temp_n > 1.0f) temp_n = 1.0f;
    float hum_n = hum / 100.0f;
    if (hum_n < 0.0f) hum_n = 0.0f;
    if (hum_n > 1.0f) hum_n = 1.0f;

    x[0] = 1.0f;
    x[1] = sinf(2.0f * (float)M_PI * hour / 24.0f);
    x[2] = cosf(2.0f * (float)M_PI * hour / 24.0f);
    x[3] = (tm_now.tm_wday == 4 || tm_now.tm_wday == 5) ? 1.0f : 0.0f;  // پنج‌شنبه/جمعه
    x[4] = presence ? 1.0f : 0.0f;
    x[5] = lux_n;
    x[6] = temp_n;
    x[7] = hum_n;
    x[8] = 0.0f;   // فراخواننده پر می‌کند
    return true;
}

// باید با s_mutex گرفته‌شده صدا زده شود
static void maybe_promote_locked(ml_dev_t *d, const char *name)
{
    if (d->mode != ML_MODE_SHADOW || s_shadow_lock) {
        return;
    }
    if (d->window_n >= ML_AUTO_MIN_N && acc_pct(d) >= ML_AUTO_ACC_PCT) {
        d->mode = ML_MODE_AUTO;
        ESP_LOGI(TAG, "[%s] shadow -> AUTO (acc=%.0f%% over %u decisions)",
                 name, acc_pct(d), (unsigned)d->window_n);
    }
}

/**
 * @brief observer ثبت‌شده روی app_state - نقطه‌ی مرکزی یادگیری.
 *        هر تغییر *از طرف کاربر* یعنی یک نمونه‌ی آموزشی تازه: هم ارزیابی
 *        پیش‌بینی (پنجره دقت) و هم یک گام SGD.
 *        تغییرات from_ml آموزش نمی‌دهند (مدل از کار خودش یاد نمی‌گیرد).
 */
static void on_device_changed(app_device_t dev, bool new_state,
                              bool prev_state, bool from_ml)
{
    (void)prev_state;
    if (from_ml || !time_sync_is_ready()) {
        return;
    }

    ml_dev_t *d = (dev == APP_DEV_LIGHT) ? &s_light : &s_fan;
    const char *name = (dev == APP_DEV_LIGHT) ? "light" : "fan";

    float x[ML_N_FEATURES];
    if (!build_features_base(x)) {
        return;
    }
    x[8] = prev_state ? 1.0f : 0.0f;

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    float p = ml_predict(&d->model, x);
    window_push(d, ((p >= 0.5f) == new_state));
    ml_train_sample(&d->model, x, new_state ? 1 : 0, ML_ONLINE_LR);
    maybe_promote_locked(d, name);
    bool mode_changed = (d->mode == ML_MODE_AUTO);
    float acc = acc_pct(d);
    uint32_t n = d->window_n;
    uint32_t upd = d->model.updates;
    s_dirty = true;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "[learn:%s] user set %s | predicted p=%.2f | acc=%.0f%% (n=%u, updates=%u)",
             name, new_state ? "ON" : "OFF", p, acc, (unsigned)n, (unsigned)upd);

    if (mode_changed) {
        // ارتقا رخ داده - وضعیت سوییچ HA را تازه کن
        mqtt_manager_publish_ml_mode(ml_agent_any_auto());
    }
}

// ---------------------------------------------------------------------
// تسک تصمیم‌گیر
// ---------------------------------------------------------------------

static void ml_task_fn(void *arg)
{
    while (!time_sync_is_ready()) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    ESP_LOGI(TAG, "Time ready - ML agent active (light=%s, fan=%s)",
             s_light.mode == ML_MODE_AUTO ? "auto" : "shadow",
             s_fan.mode == ML_MODE_AUTO ? "auto" : "shadow");

    while (true) {
        bool act_light_on = false, act_light_off = false;
        bool act_fan_on = false, act_fan_off = false;
        float p_light = 0.5f, p_fan = 0.5f;
        float a_light = 0.0f, a_fan = 0.0f;
        uint32_t n_light = 0, n_fan = 0, upd_total = 0;
        bool auto_light = false, auto_fan = false;
        bool have_features = false;

        float xb[ML_N_FEATURES];
        if (build_features_base(xb)) {
            // خواندن وضعیت فعلی *قبل* از گرفتن قفل ML (بدون hold-and-wait)
            bool cur_light = app_state_get_light();
            bool cur_fan = app_state_get_fan();

            have_features = true;

            // تصمیم‌ها داخل قفل محاسبه می‌شوند؛ عمل (که خودش observer را
            // صدا می‌زند) حتماً بیرون از قفل - وگرنه deadlock
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            float xl[ML_N_FEATURES], xf[ML_N_FEATURES];
            memcpy(xl, xb, sizeof(xb));
            memcpy(xf, xb, sizeof(xb));
            xl[8] = cur_light ? 1.0f : 0.0f;
            xf[8] = cur_fan ? 1.0f : 0.0f;

            p_light = ml_predict(&s_light.model, xl);
            p_fan = ml_predict(&s_fan.model, xf);

            if (s_light.mode == ML_MODE_AUTO) {
                if (p_light >= ML_ACT_ON_P && !cur_light)  act_light_on = true;
                if (p_light <= ML_ACT_OFF_P && cur_light)  act_light_off = true;
            }
            if (s_fan.mode == ML_MODE_AUTO) {
                if (p_fan >= ML_ACT_ON_P && !cur_fan)  act_fan_on = true;
                if (p_fan <= ML_ACT_OFF_P && cur_fan)  act_fan_off = true;
            }

            a_light = acc_pct(&s_light);  a_fan = acc_pct(&s_fan);
            n_light = s_light.window_n;   n_fan = s_fan.window_n;
            upd_total = s_light.model.updates + s_fan.model.updates;
            auto_light = (s_light.mode == ML_MODE_AUTO);
            auto_fan = (s_fan.mode == ML_MODE_AUTO);
            xSemaphoreGive(s_mutex);

            if (act_light_on)  ESP_LOGI(TAG, "[auto] light ON  (p=%.2f)", p_light);
            if (act_light_off) ESP_LOGI(TAG, "[auto] light OFF (p=%.2f)", p_light);
            if (act_fan_on)    ESP_LOGI(TAG, "[auto] fan ON    (p=%.2f)", p_fan);
            if (act_fan_off)   ESP_LOGI(TAG, "[auto] fan OFF   (p=%.2f)", p_fan);
        }

        // عمل - خارج از قفل. چک دوباره‌ی وضعیت: اگر کاربر بین تصمیم و اجرا
        // دستی عمل کرده باشد، اقدام او مقدم است و مدل عقب نمی‌نشاند.
        if (act_light_on && !app_state_get_light())   app_state_set_light_auto(true);
        if (act_light_off && app_state_get_light())   app_state_set_light_auto(false);
        if (act_fan_on && !app_state_get_fan())       app_state_set_fan_auto(true);
        if (act_fan_off && app_state_get_fan())       app_state_set_fan_auto(false);

        if (have_features) {
            mqtt_manager_publish_ml_stats(p_light, p_fan, a_light, a_fan,
                                          n_light, n_fan, upd_total,
                                          auto_light, auto_fan);
        }

        // ذخیره‌ی وزن‌ها در NVS (هر سیکل حداکثر یک‌بار)
        if (s_dirty) {
            xSemaphoreTake(s_mutex, portMAX_DELAY);
            uint8_t cfg = 0;
            if (s_light.mode == ML_MODE_AUTO) cfg |= ML_CFG_AUTO_LIGHT;
            if (s_fan.mode == ML_MODE_AUTO)   cfg |= ML_CFG_AUTO_FAN;
            if (s_shadow_lock)                cfg |= ML_CFG_SHADOW_LOCK;
            ml_persistence_save(&s_light.model, &s_fan.model, cfg);
            s_dirty = false;
            xSemaphoreGive(s_mutex);
        }

        vTaskDelay(pdMS_TO_TICKS(ML_TASK_PERIOD_MS));
    }
}

// ---------------------------------------------------------------------
// API عمومی
// ---------------------------------------------------------------------

void ml_agent_init(void)
{
    s_mutex = xSemaphoreCreateMutex();

    memset(&s_light, 0, sizeof(s_light));
    memset(&s_fan, 0, sizeof(s_fan));
    memcpy(s_light.model.w, ML_LIGHT_W, sizeof(s_light.model.w));
    memcpy(s_fan.model.w, ML_FAN_W, sizeof(s_fan.model.w));
    s_light.mode = ML_MODE_SHADOW;
    s_fan.mode = ML_MODE_SHADOW;

    uint8_t cfg = 0;
    if (ml_persistence_load(&s_light.model, &s_fan.model, &cfg)) {
        if (cfg & ML_CFG_AUTO_LIGHT) s_light.mode = ML_MODE_AUTO;
        if (cfg & ML_CFG_AUTO_FAN)   s_fan.mode = ML_MODE_AUTO;
        s_shadow_lock = (cfg & ML_CFG_SHADOW_LOCK) != 0;
        ESP_LOGI(TAG, "Restored learned models (pretrained fallback skipped)");
    } else {
        ESP_LOGI(TAG, "No valid NVS state - using pretrained weights "
                      "(light acc ~91%, fan acc ~93% on synthetic data)");
    }

    app_state_register_device_cb(on_device_changed);
    xTaskCreate(ml_task_fn, "ml_agent", ML_TASK_STACK, NULL,
                ML_TASK_PRIORITY, NULL);
}

void ml_agent_set_autonomy(bool auto_mode)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_light.mode = auto_mode ? ML_MODE_AUTO : ML_MODE_SHADOW;
    s_fan.mode = auto_mode ? ML_MODE_AUTO : ML_MODE_SHADOW;
    s_shadow_lock = !auto_mode;
    s_dirty = true;
    xSemaphoreGive(s_mutex);

    mqtt_manager_publish_ml_mode(ml_agent_any_auto());
    ESP_LOGI(TAG, "Autonomy forced via HA: %s (shadow-lock=%s)",
             auto_mode ? "AUTO" : "SHADOW", auto_mode ? "no" : "yes");
}

bool ml_agent_any_auto(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool any = (s_light.mode == ML_MODE_AUTO) || (s_fan.mode == ML_MODE_AUTO);
    xSemaphoreGive(s_mutex);
    return any;
}
