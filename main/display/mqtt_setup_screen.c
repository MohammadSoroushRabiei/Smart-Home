#include "mqtt_setup_screen.h"
#include "mqtt_manager.h"
#include "mqtt_config.h"
#include "lcd_driver.h"

#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "mqtt_setup_screen";

static lv_obj_t *s_overlay;
static lv_obj_t *s_back_btn;
static lv_obj_t *s_title;
static lv_obj_t *s_textarea;
static lv_obj_t *s_save_btn;
static lv_obj_t *s_status_label;
static lv_obj_t *s_keyboard;
static lv_timer_t *s_close_timer = NULL;

static void update_status_label(mqtt_manager_state_t state)
{
    if (s_status_label == NULL) {
        return;
    }

    switch (state) {
        case MQTT_MGR_STATE_CONNECTING:
            lv_obj_set_style_text_color(s_status_label, lv_palette_main(LV_PALETTE_ORANGE), 0);
            lv_label_set_text(s_status_label, "Connecting...");
            break;
        case MQTT_MGR_STATE_CONNECTED:
            lv_obj_set_style_text_color(s_status_label, lv_palette_main(LV_PALETTE_BLUE), 0);
            lv_label_set_text(s_status_label, "Connected");
            break;
        case MQTT_MGR_STATE_DISABLED:
            lv_obj_set_style_text_color(s_status_label, lv_color_white(), 0);
            lv_label_set_text(s_status_label, "Disabled\n(tap HA button on main screen to enable)");
            break;
        case MQTT_MGR_STATE_UNCONFIGURED:
        default:
            lv_obj_set_style_text_color(s_status_label, lv_color_white(), 0);
            lv_label_set_text(s_status_label, "Not configured yet");
            break;
    }
}

static void close_timer_cb(lv_timer_t *timer)
{
    s_close_timer = NULL;
    mqtt_setup_screen_hide();
}

// تسک پس‌زمینه‌ی تست اتصال - چون mqtt_manager_connect_and_save بلاک‌کننده
// است (تا حدود ۵ ثانیه)، هرگز از تسک LVGL صدا زده نمی‌شود.
static void save_task(void *arg)
{
    char *host = (char *)arg;
    bool ok = mqtt_manager_connect_and_save(host);
    free(host);

    lcd_driver_lvgl_lock();

    if (ok) {
        lv_obj_set_style_text_color(s_status_label, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_label_set_text(s_status_label, "Connected");

        if (s_close_timer) {
            lv_timer_del(s_close_timer);
        }
        s_close_timer = lv_timer_create(close_timer_cb, 1000, NULL);
        lv_timer_set_repeat_count(s_close_timer, 1);
    } else {
        lv_obj_set_style_text_color(s_status_label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(s_status_label, "Connection failed");
        // عمداً چیزی ذخیره نشده و صفحه بسته نمی‌شود - کاربر می‌تواند IP را
        // اصلاح و دوباره تلاش کند
    }

    lv_obj_remove_state(s_save_btn, LV_STATE_DISABLED);

    lcd_driver_lvgl_unlock();

    vTaskDelete(NULL);
}

static void save_btn_event_cb(lv_event_t *e)
{
    const char *host = lv_textarea_get_text(s_textarea);
    if (host == NULL || host[0] == '\0') {
        lv_obj_set_style_text_color(s_status_label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(s_status_label, "IP/host cannot be empty");
        return;
    }

    char *host_copy = malloc(strlen(host) + 1);
    if (host_copy == NULL) {
        return;
    }
    strcpy(host_copy, host);

    // تا پایان تست، اجازه‌ی زدن دوباره‌ی Save داده نمی‌شود
    lv_obj_add_state(s_save_btn, LV_STATE_DISABLED);
    lv_obj_set_style_text_color(s_status_label, lv_color_white(), 0);
    lv_label_set_text(s_status_label, "Connecting...");

    xTaskCreate(save_task, "mqtt_save_task", 4096, host_copy, 3, NULL);
}

static void back_btn_event_cb(lv_event_t *e)
{
    mqtt_setup_screen_hide();
}

void mqtt_setup_screen_init(void)
{
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    s_back_btn = lv_button_create(s_overlay);
    lv_obj_set_size(s_back_btn, 36, 36);
    lv_obj_align(s_back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(s_back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(s_back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    s_title = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_label_set_text(s_title, "MQTT / Home Assistant Broker");
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 15);

    s_textarea = lv_textarea_create(s_overlay);
    lv_textarea_set_one_line(s_textarea, true);
    lv_textarea_set_max_length(s_textarea, MQTT_CONFIG_HOST_MAX_LEN);
    lv_textarea_set_placeholder_text(s_textarea, "e.g. 192.168.1.50");
    lv_obj_set_width(s_textarea, 200);
    lv_obj_align(s_textarea, LV_ALIGN_TOP_MID, -35, 55);

    s_save_btn = lv_button_create(s_overlay);
    lv_obj_set_size(s_save_btn, 70, 40);
    lv_obj_align_to(s_save_btn, s_textarea, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_set_style_bg_color(s_save_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(s_save_btn, save_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *save_label = lv_label_create(s_save_btn);
    lv_label_set_text(save_label, "Save");
    lv_obj_center(save_label);

    s_status_label = lv_label_create(s_overlay);
    lv_obj_set_width(s_status_label, LV_PCT(90));
    lv_obj_set_style_text_align(s_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_status_label, "");
    lv_obj_align_to(s_status_label, s_textarea, LV_ALIGN_OUT_BOTTOM_MID, 35, 15);

    s_keyboard = lv_keyboard_create(s_overlay);
    lv_keyboard_set_textarea(s_keyboard, s_textarea);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_NUMBER);   // فقط عدد و نقطه - کافی برای IPv4
    lv_obj_set_size(s_keyboard, LV_PCT(100), 240);
    lv_obj_align(s_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_buttonmatrix_set_button_ctrl_all(s_keyboard, LV_BUTTONMATRIX_CTRL_NO_REPEAT);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "MQTT setup screen initialized");
}

void mqtt_setup_screen_show(void)
{
    char host[MQTT_CONFIG_HOST_MAX_LEN + 1] = "";
    if (mqtt_config_get_host(host, sizeof(host))) {
        lv_textarea_set_text(s_textarea, host);
    } else {
        lv_textarea_set_text(s_textarea, "");
    }

    update_status_label(mqtt_manager_get_state());

    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
}

void mqtt_setup_screen_hide(void)
{
    if (s_close_timer) {
        lv_timer_del(s_close_timer);
        s_close_timer = NULL;
    }
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void mqtt_setup_screen_notify_state_change(mqtt_manager_state_t state)
{
    if (s_overlay == NULL || lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    update_status_label(state);
}