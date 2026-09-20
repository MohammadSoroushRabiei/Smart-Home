#include "enroll_token.h"

#include <string.h>
#include <stdio.h>
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "enroll_token";

// برای هر منظور دو اسلات نگه می‌داریم: جاری + قبلی. با تولید توکن جدید،
// توکن جاری به اسلات قبلی منتقل می‌شود و تا پایان مهلت خودش معتبر می‌ماند.
// این گریس برای حالتی است که QR به‌صورت خودکار تازه‌سازی می‌شود (نمای
// حضور و غیاب) ولی کسی که QR قبلی را اسکن کرده هنوز درخواستش را نفرستاده.
typedef struct {
    char token[ENROLL_TOKEN_LEN + 1];
    int64_t expiry_us;   // 0 یعنی این اسلات خالی است
} token_slot_t;

#define PURPOSE_COUNT   2
#define SLOT_CURRENT    0
#define SLOT_PREVIOUS   1

static SemaphoreHandle_t s_mutex;
static token_slot_t s_slots[PURPOSE_COUNT][2];

static const char *purpose_name(enroll_token_purpose_t purpose)
{
    return purpose == ENROLL_TOKEN_PURPOSE_ENROLL ? "enroll" : "attendance";
}

static bool purpose_valid(enroll_token_purpose_t purpose)
{
    return purpose == ENROLL_TOKEN_PURPOSE_ENROLL ||
           purpose == ENROLL_TOKEN_PURPOSE_ATTENDANCE;
}

static int64_t purpose_validity_us(enroll_token_purpose_t purpose)
{
    return ((int64_t)(purpose == ENROLL_TOKEN_PURPOSE_ENROLL
                          ? ENROLL_TOKEN_VALID_MS
                          : ATTEND_TOKEN_VALID_MS)) * 1000;
}

esp_err_t enroll_token_init(void)
{
    memset(s_slots, 0, sizeof(s_slots));
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void enroll_token_generate(enroll_token_purpose_t purpose, char *out_token, size_t out_size)
{
    if (!purpose_valid(purpose)) {
        if (out_token != NULL && out_size > 0) {
            out_token[0] = '\0';
        }
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // عدد تصادفی ۶ رقمی (۰۰۰۰۰۰ تا ۹۹۹۹۹۹)
    uint32_t value = esp_random() % 1000000;
    char fresh[ENROLL_TOKEN_LEN + 1];
    snprintf(fresh, sizeof(fresh), "%06lu", (unsigned long)value);

    s_slots[purpose][SLOT_PREVIOUS] = s_slots[purpose][SLOT_CURRENT];
    strncpy(s_slots[purpose][SLOT_CURRENT].token, fresh, ENROLL_TOKEN_LEN);
    s_slots[purpose][SLOT_CURRENT].token[ENROLL_TOKEN_LEN] = '\0';
    s_slots[purpose][SLOT_CURRENT].expiry_us =
        esp_timer_get_time() + purpose_validity_us(purpose);

    ESP_LOGI(TAG, "New %s token generated (valid for %d seconds)",
             purpose_name(purpose),
             (int)(purpose_validity_us(purpose) / 1000000));

    if (out_token != NULL && out_size > 0) {
        strncpy(out_token, fresh, out_size - 1);
        out_token[out_size - 1] = '\0';
    }

    xSemaphoreGive(s_mutex);
}

bool enroll_token_validate(enroll_token_purpose_t purpose, const char *token)
{
    if (token == NULL || token[0] == '\0' || !purpose_valid(purpose)) {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    int64_t now = esp_timer_get_time();
    bool valid =
        (s_slots[purpose][SLOT_CURRENT].expiry_us != 0 &&
         now < s_slots[purpose][SLOT_CURRENT].expiry_us &&
         strcmp(token, s_slots[purpose][SLOT_CURRENT].token) == 0) ||
        (s_slots[purpose][SLOT_PREVIOUS].expiry_us != 0 &&
         now < s_slots[purpose][SLOT_PREVIOUS].expiry_us &&
         strcmp(token, s_slots[purpose][SLOT_PREVIOUS].token) == 0);

    xSemaphoreGive(s_mutex);

    if (!valid) {
        ESP_LOGW(TAG, "%s token validation failed (expired, mismatched, or none active)",
                 purpose_name(purpose));
    }

    return valid;
}

void enroll_token_invalidate(enroll_token_purpose_t purpose)
{
    if (!purpose_valid(purpose)) {
        return;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    memset(&s_slots[purpose], 0, sizeof(s_slots[purpose]));
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "%s tokens invalidated manually", purpose_name(purpose));
}
