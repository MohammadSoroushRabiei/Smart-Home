#include "lcd_driver.h"

#include <sys/lock.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_err.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_st7796.h"

static const char *TAG = "lcd_driver";

#define LVGL_TICK_PERIOD_MS     2
#define LVGL_TASK_STACK_SIZE    (8 * 1024)
#define LVGL_TASK_PRIORITY      2
#define LVGL_TASK_MIN_DELAY_MS  (1000 / CONFIG_FREERTOS_HZ)
#define LVGL_TASK_MAX_DELAY_MS  500
#define LVGL_DRAW_BUF_LINES     60   // بافر در RAM داخلی، نه PSRAM (برای رندر سریع‌تر)

static _lock_t s_lvgl_lock;
static lv_display_t *s_display = NULL;
static esp_lcd_panel_io_handle_t s_io_handle = NULL;
static esp_lcd_panel_handle_t s_panel_handle = NULL;

void lcd_driver_lvgl_lock(void)   { _lock_acquire(&s_lvgl_lock); }
void lcd_driver_lvgl_unlock(void) { _lock_release(&s_lvgl_lock); }

lv_display_t *lcd_driver_get_display(void) { return s_display; }

void lcd_backlight_set(bool on)
{
    gpio_set_level(LCD_PIN_BACKLIGHT, on ? 1 : 0);
}

static void init_backlight_gpio(void)
{
    gpio_config_t bk_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << LCD_PIN_BACKLIGHT,
    };
    ESP_ERROR_CHECK(gpio_config(&bk_cfg));
    lcd_backlight_set(false);
}

static bool notify_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                esp_lcd_panel_io_event_data_t *edata,
                                void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
}

static void flush_cb(lv_display_t *display, const lv_area_t *area, uint8_t *color_map)
{
    esp_lcd_panel_handle_t panel = lv_display_get_user_data(display);

    lv_draw_sw_rgb565_swap(color_map,
        (area->x2 + 1 - area->x1) * (area->y2 + 1 - area->y1));

    esp_lcd_panel_draw_bitmap(panel, area->x1, area->y1,
                               area->x2 + 1, area->y2 + 1, color_map);
}

static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_port_task(void *arg)
{
    ESP_LOGI(TAG, "Starting LVGL task");
    uint32_t delay_ms = 0;
    while (1) {
        lcd_driver_lvgl_lock();
        delay_ms = lv_timer_handler();
        lcd_driver_lvgl_unlock();

        delay_ms = MAX(delay_ms, LVGL_TASK_MIN_DELAY_MS);
        delay_ms = MIN(delay_ms, LVGL_TASK_MAX_DELAY_MS);
        vTaskDelay(pdMS_TO_TICKS(delay_ms));
    }
}

static esp_err_t init_i80_bus_and_panel(void)
{
    esp_err_t ret;

    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = LCD_PIN_DC,
        .wr_gpio_num = LCD_PIN_WR,
        .data_gpio_nums = {
            LCD_PIN_D0, LCD_PIN_D1, LCD_PIN_D2, LCD_PIN_D3,
            LCD_PIN_D4, LCD_PIN_D5, LCD_PIN_D6, LCD_PIN_D7,
        },
        .bus_width = 8,
        .max_transfer_bytes = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(uint16_t),
        .dma_burst_size = 64,
    };
    ret = esp_lcd_new_i80_bus(&bus_config, &i80_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "i80 bus init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .lcd_cmd_bits = 8,
        .lcd_param_bits = 8,
    };
    ret = esp_lcd_new_panel_io_i80(i80_bus, &io_config, &s_io_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel io init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Install ST7796 panel driver");
    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = LCD_PIN_RESET,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_BGR,
        .bits_per_pixel = 16,
    };
    ret = esp_lcd_new_panel_st7796(s_io_handle, &panel_config, &s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel driver init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_lcd_panel_reset(s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel reset failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_lcd_panel_init(s_panel_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "panel init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_lcd_panel_invert_color(s_panel_handle, false);
    esp_lcd_panel_mirror(s_panel_handle, false, true);
    esp_lcd_panel_disp_on_off(s_panel_handle, true);

    return ESP_OK;
}

static void init_lvgl(void)
{
    lv_init();

    s_display = lv_display_create(LCD_H_RES, LCD_V_RES);
    lv_display_set_user_data(s_display, s_panel_handle);
    lv_display_set_color_format(s_display, LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(s_display, flush_cb);

    size_t buf_size = LCD_H_RES * LVGL_DRAW_BUF_LINES * sizeof(lv_color16_t);
    void *buf1 = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    void *buf2 = heap_caps_malloc(buf_size, MALLOC_CAP_INTERNAL | MALLOC_CAP_DMA);
    assert(buf1 && buf2);
    lv_display_set_buffers(s_display, buf1, buf2, buf_size, LV_DISPLAY_RENDER_MODE_PARTIAL);

    const esp_timer_create_args_t tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick",
    };
    esp_timer_handle_t tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&tick_timer_args, &tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    const esp_lcd_panel_io_callbacks_t cbs = {
        .on_color_trans_done = notify_flush_ready,
    };
    ESP_ERROR_CHECK(esp_lcd_panel_io_register_event_callbacks(s_io_handle, &cbs, s_display));

    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
}

bool lcd_driver_init(void)
{
    init_backlight_gpio();

    if (init_i80_bus_and_panel() != ESP_OK) {
        ESP_LOGE(TAG, "LCD init failed - system will continue without display");
        return false;
    }

    lcd_backlight_set(true);
    init_lvgl();
    ESP_LOGI(TAG, "LCD driver initialized");
    return true;
}