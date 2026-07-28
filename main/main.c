#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BTN GPIO_NUM_1
#define LED GPIO_NUM_2

bool led_state = false;
bool btn_last_state = false;


void led_init()
{
    gpio_reset_pin(LED);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
}

void btn_init()
{
    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN, GPIO_PULLDOWN_ONLY);
}

bool btn_is_pressed(gpio_num_t btn)
{
    bool btn_current_state = gpio_get_level(btn);
        
    if (btn_current_state != btn_last_state) {
        vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
        btn_current_state = gpio_get_level(btn);
        btn_last_state = btn_current_state ;

        return btn_current_state;
    }
    
    return false;
}


void led_toggle(gpio_num_t led)
{
    led_state = !led_state ;
    gpio_set_level(led,led_state);
}

void app_main(void)
{
    led_init();
    btn_init();
    

    
    while (1)
    {
        if(btn_is_pressed(BTN))
        {
            led_toggle(LED);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}