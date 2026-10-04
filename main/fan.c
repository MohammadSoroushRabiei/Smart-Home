#include "fan.h"
#include "esp_log.h"

static const char *TAG = "FAN_LED";

static bool s_fan_on = false;

void fan_init(void)
{
    gpio_reset_pin(FAN_GPIO);
    gpio_set_direction(FAN_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_level(FAN_GPIO, 0);
    s_fan_on = false;
    ESP_LOGI(TAG, "Fan LED on GPIO%d ready (OFF)", (int)FAN_GPIO);
}

void fan_set(bool on)
{
    s_fan_on = on;
    gpio_set_level(FAN_GPIO, on);
    ESP_LOGI(TAG, "%s", on ? "ON" : "OFF");
}

bool fan_is_on(void)
{
    return s_fan_on;
}
