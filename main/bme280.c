#include "bme280.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "bme280";

// اگر پین SDO به GND وصل باشد آدرس 0x76 است، اگر به VCC باشد 0x77
#define BME280_I2C_ADDR      0x76

#define BME280_REG_ID        0xD0
#define BME280_REG_CTRL_HUM  0xF2
#define BME280_REG_STATUS    0xF3
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG    0xF5
#define BME280_REG_PRESS_MSB 0xF7
#define BME280_REG_CALIB00   0x88
#define BME280_REG_DIG_H1    0xA1
#define BME280_REG_CALIB26   0xE1
#define BME280_CHIP_ID       0x60

typedef struct {
    uint16_t dig_T1;
    int16_t  dig_T2, dig_T3;
    uint16_t dig_P1;
    int16_t  dig_P2, dig_P3, dig_P4, dig_P5, dig_P6, dig_P7, dig_P8, dig_P9;
    uint8_t  dig_H1;
    int16_t  dig_H2;
    uint8_t  dig_H3;
    int16_t  dig_H4, dig_H5;
    int8_t   dig_H6;
} bme280_calib_t;

static i2c_master_dev_handle_t s_dev = NULL;
static bme280_calib_t s_calib;
static bool s_ready = false;

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *data, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, data, len, 100);
}

static void read_calibration(void)
{
    uint8_t buf[26];
    reg_read(BME280_REG_CALIB00, buf, sizeof(buf));

    s_calib.dig_T1 = (uint16_t)(buf[1] << 8 | buf[0]);
    s_calib.dig_T2 = (int16_t)(buf[3] << 8 | buf[2]);
    s_calib.dig_T3 = (int16_t)(buf[5] << 8 | buf[4]);
    s_calib.dig_P1 = (uint16_t)(buf[7] << 8 | buf[6]);
    s_calib.dig_P2 = (int16_t)(buf[9] << 8 | buf[8]);
    s_calib.dig_P3 = (int16_t)(buf[11] << 8 | buf[10]);
    s_calib.dig_P4 = (int16_t)(buf[13] << 8 | buf[12]);
    s_calib.dig_P5 = (int16_t)(buf[15] << 8 | buf[14]);
    s_calib.dig_P6 = (int16_t)(buf[17] << 8 | buf[16]);
    s_calib.dig_P7 = (int16_t)(buf[19] << 8 | buf[18]);
    s_calib.dig_P8 = (int16_t)(buf[21] << 8 | buf[20]);
    s_calib.dig_P9 = (int16_t)(buf[23] << 8 | buf[22]);

    uint8_t h1 = 0;
    reg_read(BME280_REG_DIG_H1, &h1, 1);
    s_calib.dig_H1 = h1;

    uint8_t hb[7];
    reg_read(BME280_REG_CALIB26, hb, sizeof(hb));
    s_calib.dig_H2 = (int16_t)((hb[1] << 8) | hb[0]);
    s_calib.dig_H3 = hb[2];
    s_calib.dig_H4 = (int16_t)((hb[3] << 4) | (hb[4] & 0x0F));
    s_calib.dig_H5 = (int16_t)((hb[5] << 4) | (hb[4] >> 4));
    s_calib.dig_H6 = (int8_t)hb[6];
}

bool bme280_init(i2c_master_bus_handle_t bus)
{
    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = BME280_I2C_ADDR,
        .scl_speed_hz = 100000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &s_dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "add device failed: %s", esp_err_to_name(ret));
        return false;
    }

    uint8_t chip_id = 0;
    ret = reg_read(BME280_REG_ID, &chip_id, 1);
    if (ret != ESP_OK || chip_id != BME280_CHIP_ID) {
        ESP_LOGE(TAG, "BME280 not found (chip_id=0x%02X, err=%s)", chip_id, esp_err_to_name(ret));
        return false;
    }

    read_calibration();

    reg_write(BME280_REG_CTRL_HUM, 0x01);   // oversampling رطوبت x1
    reg_write(BME280_REG_CONFIG, 0x00);     // filter خاموش (در forced mode اهمیتی ندارد)

    s_ready = true;
    ESP_LOGI(TAG, "BME280 initialized (chip_id=0x%02X)", chip_id);
    return true;
}

bool bme280_read(bme280_data_t *out)
{
    if (!s_ready || out == NULL) {
        return false;
    }

    reg_write(BME280_REG_CTRL_MEAS, 0x25);  // osrs_t=1, osrs_p=1, mode=forced

    for (int i = 0; i < 20; i++) {
        uint8_t status = 0;
        reg_read(BME280_REG_STATUS, &status, 1);
        if ((status & 0x08) == 0) {
            break;
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }

    uint8_t raw[8];
    if (reg_read(BME280_REG_PRESS_MSB, raw, sizeof(raw)) != ESP_OK) {
        ESP_LOGW(TAG, "burst read failed");
        return false;
    }

    int32_t adc_P = (int32_t)((raw[0] << 12) | (raw[1] << 4) | (raw[2] >> 4));
    int32_t adc_T = (int32_t)((raw[3] << 12) | (raw[4] << 4) | (raw[5] >> 4));
    int32_t adc_H = (int32_t)((raw[6] << 8) | raw[7]);

    // فرمول‌های کمپانزیشن double-precision (طبق دیتاشیت Bosch BME280)
    double var1, var2, T, P = 0, H;
    int32_t t_fine;

    var1 = (((double)adc_T) / 16384.0 - ((double)s_calib.dig_T1) / 1024.0) * ((double)s_calib.dig_T2);
    var2 = ((((double)adc_T) / 131072.0 - ((double)s_calib.dig_T1) / 8192.0) *
            (((double)adc_T) / 131072.0 - ((double)s_calib.dig_T1) / 8192.0)) * ((double)s_calib.dig_T3);
    t_fine = (int32_t)(var1 + var2);
    T = (var1 + var2) / 5120.0;

    var1 = ((double)t_fine / 2.0) - 64000.0;
    var2 = var1 * var1 * ((double)s_calib.dig_P6) / 32768.0;
    var2 = var2 + var1 * ((double)s_calib.dig_P5) * 2.0;
    var2 = (var2 / 4.0) + (((double)s_calib.dig_P4) * 65536.0);
    var1 = (((double)s_calib.dig_P3) * var1 * var1 / 524288.0 + ((double)s_calib.dig_P2) * var1) / 524288.0;
    var1 = (1.0 + var1 / 32768.0) * ((double)s_calib.dig_P1);
    if (var1 != 0.0) {
        P = 1048576.0 - (double)adc_P;
        P = (P - (var2 / 4096.0)) * 6250.0 / var1;
        var1 = ((double)s_calib.dig_P9) * P * P / 2147483648.0;
        var2 = P * ((double)s_calib.dig_P8) / 32768.0;
        P = P + (var1 + var2 + ((double)s_calib.dig_P7)) / 16.0;
    }

    H = ((double)t_fine) - 76800.0;
    H = (adc_H - (((double)s_calib.dig_H4) * 64.0 + ((double)s_calib.dig_H5) / 16384.0 * H)) *
        (((double)s_calib.dig_H2) / 65536.0 * (1.0 + ((double)s_calib.dig_H6) / 67108864.0 * H *
        (1.0 + ((double)s_calib.dig_H3) / 67108864.0 * H)));
    H = H * (1.0 - ((double)s_calib.dig_H1) * H / 524288.0);
    if (H > 100.0) H = 100.0;
    if (H < 0.0)   H = 0.0;

    out->temperature_c    = (float)T;
    out->pressure_hpa     = (float)(P / 100.0);
    out->humidity_percent = (float)H;
    return true;
}