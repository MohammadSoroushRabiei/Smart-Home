#include <stdio.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"

#define BTN GPIO_NUM_1
#define LED GPIO_NUM_2


// void btn_clicked(gpio_num_t GPIO_NUM)
// {

// }

void app_main(void)
{
    gpio_reset_pin(LED);
    gpio_reset_pin(BTN);
    gpio_set_direction(LED, GPIO_MODE_OUTPUT);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN, GPIO_PULLDOWN_ONLY);

    bool btn_last_state = false;
    bool led_state = false;
    
    while (1)
    {
        bool btn_current_state = gpio_get_level(BTN);
        
        if (btn_current_state != btn_last_state) {
            vTaskDelay(pdMS_TO_TICKS(50)); // Debounce
            btn_current_state = gpio_get_level(BTN);
            if (btn_current_state == 1) {
                led_state = !led_state;
                gpio_set_level(LED, led_state);
            }
            btn_last_state = btn_current_state ;
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}