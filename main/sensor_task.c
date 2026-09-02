#include "sensor_task.h"
#include "bme280.h"
#include "app_state.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define SENSOR_READ_PERIOD_MS  5000
#define SENSOR_TASK_STACK      3072
#define SENSOR_TASK_PRIORITY   2

static const char *TAG = "sensor_task";

static void sensor_task_fn(void *arg)
{
    while (1) {
        bme280_data_t data;
        if (bme280_read(&data)) {
            app_state_set_sensor_data(data.temperature_c, data.humidity_percent, data.pressure_hpa);
        } else {
            ESP_LOGW(TAG, "BME280 read failed");
        }
        vTaskDelay(pdMS_TO_TICKS(SENSOR_READ_PERIOD_MS));
    }
}

void sensor_task_start(void)
{
    xTaskCreate(sensor_task_fn, "sensor_task", SENSOR_TASK_STACK, NULL, SENSOR_TASK_PRIORITY, NULL);
}