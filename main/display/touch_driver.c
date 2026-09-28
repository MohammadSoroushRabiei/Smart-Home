#include "touch_driver.h"
#include "lcd_driver.h"
#include "i2c_bus.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_touch_gt911.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"

static const char *TAG = "touch_driver";

#define TOUCH_POLL_PERIOD_MS        30
#define TOUCH_POLL_TASK_STACK       4096
#define TOUCH_POLL_TASK_PRIORITY    1     // پایین‌تر از تسک LVGL (که 2 است) تا رندر را عقب نیندازد
#define TOUCH_ERROR_BACKOFF_COUNT   5
#define TOUCH_ERROR_BACKOFF_MS      2000

// GT911 در IDF v6 از طریق esp_lcd با timeout بی‌نهایت (‎-1‎) با باس حرف می‌زند؛
// اگر چیپ بعد از یک glitch خط SDA را پایین نگه دارد، تراکنش هیچ‌وقت برنمی‌گردد.
// تسک ناظر با تپش قلبِ poll این حالت را می‌فهمد و GT911 را با پالس سخت‌افزاری
// روی پایه RST (همان توالی init درایور) ریست می‌کند تا باس آزاد شود - بدون ریبوت
#define TOUCH_WATCHDOG_CHECK_MS         2000
#define TOUCH_STUCK_HEARTBEAT_MS        6000   // سکوت heartbeat = گیر کردن داخل تراکنش
#define TOUCH_FAIL_STREAK_RECOVER       50     // خطای متوالی (مثلاً NACK همیشگی) هم ریکاوری را رقم می‌زند
#define TOUCH_RECOVER_MIN_INTERVAL_MS   10000
#define TOUCH_WATCHDOG_MAX_BLIND        5      // بعد از این تعداد ریکاوری بی‌نتیجه، تور نجات: TWDT panic
#define TOUCH_WATCHDOG_TASK_STACK       3072
#define TOUCH_WATCHDOG_TASK_PRIORITY    3      // بالاتر از touch_poll تا هنگام اسپین I2C هم اجرا شود

static esp_lcd_touch_handle_t s_tp_handle = NULL;
static i2c_master_bus_handle_t s_i2c_bus = NULL;   // باس تاچ (I2C_NUM_0) - برای قفل باس
static uint8_t s_gt911_addr = 0x5D;   // آدرس بایندشده - ریست سخت‌افزاری هم باید همین را دوباره انتخاب کند

// نتیجه‌ی خوانده‌شده توسط تسک پس‌زمینه؛ callback فقط این‌ها را می‌خواند (بدون I2C)
static volatile bool     s_touch_pressed = false;
static volatile uint16_t s_touch_x = 0;
static volatile uint16_t s_touch_y = 0;

// تپش قلب تسک poll (به‌روزرسانی در ابتدای هر دور حلقه) و شمارش خطاهای متوالی
static volatile int64_t  s_last_poll_us = 0;
static volatile uint32_t s_fail_streak = 0;
// ریکاوری‌های بی‌نتیجه‌ی پشت‌سرهم - فقط در تسک ناظر دستکاری می‌شود
static int s_blind_recoveries = 0;

static void touch_poll_task(void *arg)
{
    uint8_t consecutive_errors = 0;

    while (1) {
        s_last_poll_us = esp_timer_get_time();

        i2c_bus_lock(s_i2c_bus);
        esp_err_t ret = esp_lcd_touch_read_data(s_tp_handle);
        i2c_bus_unlock(s_i2c_bus);
        if (ret == ESP_OK) {
            consecutive_errors = 0;
            s_fail_streak = 0;

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
            s_fail_streak++;
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

// همان توالی ریست/انتخاب آدرس در esp_lcd_touch_gt911 (INT پایین هنگام بالا آمدن
// RST یعنی آدرس 0x5D) تا GT911 بعد از ریست روی همان آدرس قبلی بالا بیاید و
// هندل موجود درایور معتبر بماند و نیاز به ساخت مجدد panel_io نباشد
static void gt911_hw_reset_for_recovery(void)
{
    gpio_config_t int_out_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .intr_type = GPIO_INTR_DISABLE,
        .pull_up_en = 1,
        .pin_bit_mask = BIT64(TOUCH_PIN_INT),
    };
    gpio_config(&int_out_cfg);

    gpio_set_level(TOUCH_PIN_RST, 0);   // levels.reset = 0
    gpio_set_level(TOUCH_PIN_INT, (s_gt911_addr == 0x14) ? 1 : 0);   // سطح INT لحظه‌ی بالا آمدن RST آدرس را انتخاب می‌کند
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(TOUCH_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    vTaskDelay(pdMS_TO_TICKS(50));

    // بازگرداندن INT به حالت ورودی - مثل انتهای init درایور
    gpio_config_t int_in_cfg = {
        .mode = GPIO_MODE_INPUT,
        .intr_type = GPIO_INTR_NEGEDGE,
        .pin_bit_mask = BIT64(TOUCH_PIN_INT),
    };
    gpio_config(&int_in_cfg);

    ESP_LOGW(TAG, "GT911 hardware reset issued to release stuck I2C bus");
}

static void touch_watchdog_task(void *arg)
{
    int64_t last_recover_us = 0;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(TOUCH_WATCHDOG_CHECK_MS));

        int64_t now = esp_timer_get_time();
        bool stuck = (now - s_last_poll_us) > ((int64_t)TOUCH_STUCK_HEARTBEAT_MS * 1000);
        bool failing = s_fail_streak >= TOUCH_FAIL_STREAK_RECOVER;

        if (!stuck && !failing) {
            s_blind_recoveries = 0;
            continue;
        }
        // اگر ریست‌های مکرر هم heartbeat را برنگرداندند، کاری از دست ناظر برنمی‌آید؛
        // ول کن تا Task Watchdog با panic دستگاه را ریبوت کند (آخرین خط دفاع)
        if (stuck && s_blind_recoveries >= TOUCH_WATCHDOG_MAX_BLIND) {
            continue;
        }
        if ((now - last_recover_us) < ((int64_t)TOUCH_RECOVER_MIN_INTERVAL_MS * 1000)) {
            continue;
        }

        last_recover_us = now;
        if (stuck) {
            s_blind_recoveries++;
        }
        gt911_hw_reset_for_recovery();
        s_fail_streak = 0;
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
bool touch_driver_init(lv_display_t *disp, i2c_master_bus_handle_t i2c_bus)
{
    esp_err_t ret;
    s_i2c_bus = i2c_bus;

    esp_lcd_panel_io_handle_t tp_io_handle = NULL;
    esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();

    // GT911 آدرس I2C‌اش را از سطح INT در لحظه‌ی ریست انتخاب می‌کند (پایین=0x5D، بالا=0x14).
    // اگر INT هنگام پاور-آپ شناور باشد ممکن است چیپ روی آدرس دیگر بالا بیاید؛
    // اول probe می‌زنیم، اگر هیچ‌کدام جواب نداد با توالی ریستِ سخت‌افزاری آدرس را
    // صریح انتخاب می‌کنیم و دوباره probe می‌زنیم.
    static const uint8_t k_gt911_addrs[2] = { 0x5D, 0x14 };
    for (int attempt = 0; attempt < 2 && s_gt911_addr == 0; attempt++) {
        if (attempt > 0) {
            ESP_LOGW(TAG, "GT911 silent on bus, issuing address-select hardware reset and retrying");
            gt911_hw_reset_for_recovery();
            vTaskDelay(pdMS_TO_TICKS(60));
        }
        for (int i = 0; i < 2; i++) {
            if (i2c_master_probe(i2c_bus, k_gt911_addrs[i], 100) == ESP_OK) {
                s_gt911_addr = k_gt911_addrs[i];
                break;
            }
        }
    }

    if (s_gt911_addr == 0) {
        ESP_LOGE(TAG, "GT911 answers on neither 0x5D nor 0x14 - check touch VDD/GND, SDA->GPIO1, SCL->GPIO2 and the 4.7k pull-ups");
        return false;
    }
    if (s_gt911_addr != tp_io_config.dev_addr) {
        ESP_LOGW(TAG, "GT911 woke up at alternate address 0x%02X - binding there", s_gt911_addr);
        tp_io_config.dev_addr = s_gt911_addr;
    }

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
        .levels = { .reset = 0, .interrupt = 0 },
        .flags = { .swap_xy = 0, .mirror_x = 1, .mirror_y = 1 },
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

    xTaskCreate(touch_watchdog_task, "touch_wdt", TOUCH_WATCHDOG_TASK_STACK, NULL,
                TOUCH_WATCHDOG_TASK_PRIORITY, NULL);

    ESP_LOGI(TAG, "Touch driver initialized");
    return true;
}