#include "i2c_bus.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

static const char *TAG = "i2c_bus";

static i2c_master_bus_handle_t s_touch_bus  = NULL;
static i2c_master_bus_handle_t s_sensor_bus = NULL;
static SemaphoreHandle_t s_touch_mutex  = NULL;
static SemaphoreHandle_t s_sensor_mutex = NULL;

static bool bus_create(int sda, int scl, i2c_port_t port, i2c_master_bus_handle_t *out)
{
    i2c_master_bus_config_t conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = sda,
        .scl_io_num = scl,
        .i2c_port = port,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&conf, out);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed (port=%d, SDA=%d, SCL=%d): %s",
                 port, sda, scl, esp_err_to_name(ret));
        return false;
    }
    return true;
}

bool i2c_bus_init(void)
{
    s_touch_mutex = xSemaphoreCreateMutex();
    s_sensor_mutex = xSemaphoreCreateMutex();
    if (s_touch_mutex == NULL || s_sensor_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create I2C bus mutexes");
        return false;
    }

    if (!bus_create(TOUCH_I2C_SDA, TOUCH_I2C_SCL, I2C_NUM_0, &s_touch_bus)) {
        return false;
    }
    if (!bus_create(SENSOR_I2C_SDA, SENSOR_I2C_SCL, I2C_NUM_1, &s_sensor_bus)) {
        ESP_LOGW(TAG, "Continuing without sensor I2C bus");
    }

    ESP_LOGI(TAG, "Touch bus: SDA=%d/SCL=%d | Sensor bus: SDA=%d/SCL=%d",
             TOUCH_I2C_SDA, TOUCH_I2C_SCL, SENSOR_I2C_SDA, SENSOR_I2C_SCL);
    return true;
}

i2c_master_bus_handle_t i2c_bus_touch_handle(void)
{
    return s_touch_bus;
}

i2c_master_bus_handle_t i2c_bus_sensor_handle(void)
{
    return s_sensor_bus;
}

void i2c_bus_lock(i2c_master_bus_handle_t bus)
{
    xSemaphoreTake(bus == s_sensor_bus ? s_sensor_mutex : s_touch_mutex, portMAX_DELAY);
}

void i2c_bus_unlock(i2c_master_bus_handle_t bus)
{
    xSemaphoreGive(bus == s_sensor_bus ? s_sensor_mutex : s_touch_mutex);
}
