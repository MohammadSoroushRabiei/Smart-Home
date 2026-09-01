#include "touch_driver.h"
#include "lcd_driver.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"

static const char *TAG = "touch_driver";

#define TOUCH_POLL_PERIOD_MS        30
#define TOUCH_POLL_TASK_STACK       4096
#define TOUCH_POLL_TASK_PRIORITY    1     // پایین‌تر از تسک LVGL (که 2 است) تا رندر را عقب نیندازد
#define TOUCH_ERROR_BACKOFF_COUNT   5
#define TOUCH_ERROR_BACKOFF_MS      2000

static esp_lcd_touch_handle_t s_tp_handle = NULL;

// نتیجه‌ی خوانده‌شده توسط تسک پس‌زمینه؛ callback فقط این‌ها را می‌خواند (بدون I2C)
static volatile bool     s_touch_pressed = false;
static volatile uint16_t s_touch_x = 0;
static volatile uint16_t s_touch_y = 0;

static void touch_poll_task(void *arg)
{
    uint8_t consecutive_errors = 0;

    while (1) {
        esp_err_t ret = esp_lcd_touch_read_data(s_tp_handle);

        if (ret == ESP_OK) {
            consecutive_errors = 0;

            esp_lcd_touch_point_data_t pt[1];
            uint8_t cnt = 0;
            esp_lcd_touch_get_data(s_tp_handle, pt, &cnt, 1);

            if (cnt > 0) {
                s_touch_x = pt[0].x;
                s_touch_y = pt[0].y;
                s_touch_pressed = true;
            } else {
                s_touch_pressed = false;
            }
        } else {
            consecutive_errors++;
            s_touch_pressed = false;   // در حالت خطا، لمس را رهاشده در نظر بگیر

            if (consecutive_errors >= TOUCH_ERROR_BACKOFF_COUNT) {
                ESP_LOGW(TAG, "Touch read failing repeatedly, backing off");
                vTaskDelay(pdMS_TO_TICKS(TOUCH_ERROR_BACKOFF_MS));
                consecutive_errors = 0;   // بعد از استراحت، دوباره امتحان کن
            }
        }

        vTaskDelay(pdMS_TO_TICKS(TOUCH_POLL_PERIOD_MS));
    }
}

// این تابع دیگر هیچ I2C ای صدا نمی‌زند - فقط مقادیر کش‌شده را می‌خواند (سریع، بدون بلاک شدن)
static void touch_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
    if (s_touch_pressed) {
        data->point.x = s_touch_x;
        data->point.y = s_touch_y;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

bool touch_driver_init(lv_display_t *disp)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initialize I2C bus for touch");
    i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = TOUCH_PIN_SDA,
        .scl_io_num = TOUCH_PIN_SCL,
        .i2c_port = I2C_NUM_0,
        .flags.enable_internal_pullup = true,
    };
    i2c_master_bus_handle_t i2c_bus;
    ret = i2c_new_master_bus(&i2c_bus_conf, &i2c_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "I2C bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
    ret = esp_lcd_new_panel_io_i2c(i2c_bus, &tp_io_config, &tp_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Touch panel IO init failed: %s", esp_err_to_name(ret));
        return false;
    }

    esp_lcd_touch_io_gt911_config_t tp_gt911_config = {
        .dev_addr = tp_io_config.dev_addr,
    };

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = LCD_H_RES,
        .y_max = LCD_V_RES,
        .rst_gpio_num = TOUCH_PIN_RST,
        .int_gpio_num = TOUCH_PIN_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 1,
            .mirror_y = 0,
        },
        .driver_data = &tp_gt911_config,
    };
    ret = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, &s_tp_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "GT911 init failed: %s", esp_err_to_name(ret));
        return false;
    }

    lv_indev_t *indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touch_read_cb);
    lv_timer_set_period(lv_indev_get_read_timer(indev), 30);
    lv_indev_set_display(indev, disp);

    xTaskCreate(touch_poll_task, "touch_poll", TOUCH_POLL_TASK_STACK, NULL,
                TOUCH_POLL_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Touch driver initialized");
    return true;
}