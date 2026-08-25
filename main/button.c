#include "button.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"


static bool btn_last_state = false;


void btn_init(void)
{
    gpio_reset_pin(BTN);
    gpio_set_direction(BTN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(BTN, GPIO_PULLDOWN_ONLY);
}

bool btn_is_pressed(gpio_num_t btn)
{
    bool btn_current_state = gpio_get_level(btn);

    if (btn_current_state != btn_last_state)
    {

        vTaskDelay(pdMS_TO_TICKS(50));

        btn_current_state = gpio_get_level(btn);

        btn_last_state = btn_current_state;

        return btn_current_state;
    }

    return false;
}