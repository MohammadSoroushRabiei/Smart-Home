#include "ml_model.h"
#include "ml_model_weights.h"

#include <math.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"

static const char *TAG = "ml_model";

#define ML_NVS_NAMESPACE  "mlbrain"
#define ML_NVS_VER        "ver"
#define ML_NVS_LIGHT_W    "lw"
#define ML_NVS_FAN_W      "fw"
#define ML_NVS_LIGHT_N    "ln"
#define ML_NVS_FAN_N      "fn"
#define ML_NVS_CFG        "cfg"

// پارامترهای یادگیری آنلاین (کوچک نگه داشته شده تا واکنش‌های تک‌نمونه‌ای
// ناگهانی وزن‌ها را نبرند؛ NVS هم هر چند به‌روزرسانی یک‌بار ذخیره می‌شود)
#define ML_ONLINE_LR      0.08f
#define ML_W_CLIP         8.0f

float ml_predict(const ml_linear_t *m, const float x[ML_N_FEATURES])
{
    float z = 0.0f;
    for (int i = 0; i < ML_N_FEATURES; i++) {
        z += m->w[i] * x[i];
    }
    if (z > 30.0f) z = 30.0f;
    if (z < -30.0f) z = -30.0f;
    return 1.0f / (1.0f + expf(-z));
}

void ml_train_sample(ml_linear_t *m, const float x[ML_N_FEATURES], int y, float lr)
{
    float p = ml_predict(m, x);
    float err = (float)y - p;
    for (int i = 0; i < ML_N_FEATURES; i++) {
        float w = m->w[i] + lr * err * x[i];
        if (w > ML_W_CLIP)  w = ML_W_CLIP;
        if (w < -ML_W_CLIP) w = -ML_W_CLIP;
        m->w[i] = w;
    }
    m->updates++;
}

bool ml_persistence_load(ml_linear_t *light, ml_linear_t *fan, uint8_t *cfg_flags)
{
    nvs_handle_t h;
    if (nvs_open(ML_NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return false;
    }

    uint8_t ver = 0;
    bool ok = (nvs_get_u8(h, ML_NVS_VER, &ver) == ESP_OK) && (ver == ML_MODEL_VERSION);

    size_t len = 0;
    if (ok) {
        len = sizeof(light->w);
        ok = (nvs_get_blob(h, ML_NVS_LIGHT_W, light->w, &len) == ESP_OK) &&
             (len == sizeof(light->w));
    }
    if (ok) {
        len = sizeof(fan->w);
        ok = (nvs_get_blob(h, ML_NVS_FAN_W, fan->w, &len) == ESP_OK) &&
             (len == sizeof(fan->w));
    }
    if (ok) {
        ok = (nvs_get_u32(h, ML_NVS_LIGHT_N, &light->updates) == ESP_OK) &&
             (nvs_get_u32(h, ML_NVS_FAN_N, &fan->updates) == ESP_OK);
    }
    if (ok && cfg_flags != NULL) {
        if (nvs_get_u8(h, ML_NVS_CFG, cfg_flags) != ESP_OK) {
            *cfg_flags = 0;   // قدیمی/ناموجود - پیش‌فرض: هر دو سایه
        }
    }

    nvs_close(h);
    if (ok) {
        ESP_LOGI(TAG, "Loaded learned weights from NVS (light updates=%u, fan updates=%u)",
                 (unsigned)light->updates, (unsigned)fan->updates);
    }
    return ok;
}

bool ml_persistence_save(const ml_linear_t *light, const ml_linear_t *fan, uint8_t cfg_flags)
{
    nvs_handle_t h;
    if (nvs_open(ML_NVS_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "NVS open for write failed");
        return false;
    }

    bool ok = true;
    ok &= (nvs_set_u8(h, ML_NVS_VER, ML_MODEL_VERSION) == ESP_OK);
    ok &= (nvs_set_blob(h, ML_NVS_LIGHT_W, light->w, sizeof(light->w)) == ESP_OK);
    ok &= (nvs_set_blob(h, ML_NVS_FAN_W, fan->w, sizeof(fan->w)) == ESP_OK);
    ok &= (nvs_set_u32(h, ML_NVS_LIGHT_N, light->updates) == ESP_OK);
    ok &= (nvs_set_u32(h, ML_NVS_FAN_N, fan->updates) == ESP_OK);
    ok &= (nvs_set_u8(h, ML_NVS_CFG, cfg_flags) == ESP_OK);

    if (ok) {
        ok = (nvs_commit(h) == ESP_OK);
    }
    nvs_close(h);

    if (!ok) {
        ESP_LOGE(TAG, "NVS save failed");
    }
    return ok;
}
