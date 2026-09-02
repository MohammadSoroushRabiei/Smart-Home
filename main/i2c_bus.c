#include "i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "i2c_bus";
static i2c_master_bus_handle_t s_bus = NULL;

bool i2c_bus_init(void)
{
    i2c_master_bus_config_t conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = SHARED_I2C_SDA,
        .scl_io_num = SHARED_I2C_SCL,
        .i2c_port = I2C_NUM_0,
        .flags.enable_internal_pullup = true,
    };

    esp_err_t ret = i2c_new_master_bus(&conf, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    ESP_LOGI(TAG, "Shared I2C bus initialized (SDA=%d, SCL=%d)", SHARED_I2C_SDA, SHARED_I2C_SCL);
    return true;
}

i2c_master_bus_handle_t i2c_bus_get_handle(void)
{
    return s_bus;
}