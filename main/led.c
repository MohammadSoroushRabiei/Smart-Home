#include "led.h"
#include "esp_log.h"

static bool led_state = false;

static const char * TAG_LED = "LED Status";

void led_init(void)
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
}


void led_toggle(gpio_num_t led)
{
    led_state = !led_state ;
    gpio_set_level(led,led_state);
    ESP_LOGI(TAG_LED , "%s" , led_state ? "ON" : "OFF");
}


void led_set(gpio_num_t led, bool on)
{
    led_state = on;
    gpio_set_level(led, led_state);
    ESP_LOGI(TAG_LED, "%s", led_state ? "ON" : "OFF");
}


bool led_is_on(void)
{
    return led_state;
}