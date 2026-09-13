#include "enroll_token.h"

#include <string.h>
#include <stdio.h>
#include "esp_random.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "enroll_token";

static SemaphoreHandle_t s_mutex;
static char s_token[ENROLL_TOKEN_LEN + 1] = "";
static int64_t s_expiry_us = 0;   // 0 یعنی توکنی فعال نیست

esp_err_t enroll_token_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    if (s_mutex == NULL) {
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void enroll_token_generate(char *out_token, size_t out_size)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // عدد تصادفی ۶ رقمی (۰۰۰۰۰۰ تا ۹۹۹۹۹۹)
    uint32_t value = esp_random() % 1000000;
    snprintf(s_token, sizeof(s_token), "%06lu", (unsigned long)value);
    s_expiry_us = esp_timer_get_time() + ((int64_t)ENROLL_TOKEN_VALID_MS * 1000);

    ESP_LOGI(TAG, "New enrollment token generated (valid for %d seconds)",
             ENROLL_TOKEN_VALID_MS / 1000);

    if (out_token != NULL && out_size > 0) {
        strncpy(out_token, s_token, out_size - 1);
        out_token[out_size - 1] = '\0';
    }

    xSemaphoreGive(s_mutex);
}

bool enroll_token_validate(const char *token)
{
    if (token == NULL || token[0] == '\0') {
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    bool valid = false;
    if (s_expiry_us != 0 && esp_timer_get_time() < s_expiry_us) {
        valid = (strcmp(token, s_token) == 0);
    }

    xSemaphoreGive(s_mutex);

    if (!valid) {
        ESP_LOGW(TAG, "Token validation failed (expired, mismatched, or none active)");
    }

    return valid;
}

void enroll_token_invalidate(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_token[0] = '\0';
    s_expiry_us = 0;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Token invalidated manually");
}