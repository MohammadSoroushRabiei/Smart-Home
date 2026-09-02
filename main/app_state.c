#include "app_state.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"

static const char *TAG = "app_state";

static SemaphoreHandle_t s_mutex;
static bool s_light_on = false;

void app_state_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
}

bool app_state_get_light(void)
{
    bool value;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    value = s_light_on;
    xSemaphoreGive(s_mutex);
    return value;
}

void app_state_set_light(bool on)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_light_on = on;
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "Light set to: %s", on ? "ON" : "OFF");

    // در گام‌های بعدی: اتصال به LED واقعی، آپدیت LCD، و غیره اینجا اضافه می‌شود
}