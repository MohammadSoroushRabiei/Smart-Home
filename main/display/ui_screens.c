#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "lcd_driver.h"
#include "wifi_manager.h"
#include "app_state.h"
#include "keypad_screen.h"
#include "esp_log.h"
#include "mqtt_manager.h"

static const char *TAG = "ui_screens";

static lv_obj_t *s_wifi_status_label;
static lv_obj_t *s_wifi_ip_label;
static lv_obj_t *s_retry_btn;
static lv_obj_t *s_light_btn;
static lv_obj_t *s_light_label;
static lv_obj_t *s_unlock_btn;
static lv_obj_t *s_settings_btn;
static lv_obj_t *s_sensor_label;
static lv_obj_t *s_qr_code;


static const char *wifi_state_to_display_text(wifi_state_t state)
{
    switch (state) {
        case WIFI_STATE_OFFLINE:    return "WiFi: Offline";
        case WIFI_STATE_CONNECTING: return "WiFi: Connecting...";
        case WIFI_STATE_CONNECTED:  return "WiFi: Connected";
        default:                    return "WiFi: Unknown";
    }
}

static void retry_btn_event_cb(lv_event_t *e)
{
    wifi_retry_connect();
}

static void light_btn_event_cb(lv_event_t *e)
{
    app_state_set_light(!app_state_get_light());
}

// نتیجه‌ی ورود رمز - چه از دکمه‌ی Unlock بیاد چه از دکمه‌ی Settings
static void on_keypad_result(keypad_purpose_t purpose, bool success)
{
    if (purpose == KEYPAD_PURPOSE_UNLOCK) {
        mqtt_manager_publish_access_event(success);
        if (success) {
            ESP_LOGI(TAG, "Unlock code correct - ACCESS GRANTED");
            app_state_set_lock(true);
        } else {
            ESP_LOGW(TAG, "Unlock code incorrect - ACCESS DENIED");
        }
    } else { // KEYPAD_PURPOSE_SETTINGS
        if (success) {
            // TODO(بخش ۲.۵ - صفحه‌ی تنظیمات): وقتی صفحه‌ی تنظیمات ساخته شد،
            // اینجا باید آن صفحه نمایش داده شود.
            ESP_LOGI(TAG, "Settings code correct - settings screen not implemented yet");
        } else {
            ESP_LOGW(TAG, "Settings code incorrect - ACCESS DENIED");
        }
    }
}

static void unlock_btn_event_cb(lv_event_t *e)
{
    keypad_screen_show(KEYPAD_PURPOSE_UNLOCK, on_keypad_result);
}

static void settings_btn_event_cb(lv_event_t *e)
{
    keypad_screen_show(KEYPAD_PURPOSE_SETTINGS, on_keypad_result);
}

void ui_screens_init(void)
{
    lv_obj_t *scr = lv_screen_active();

    s_wifi_status_label = lv_label_create(scr);
    lv_label_set_text(s_wifi_status_label, "WiFi: Offline");
    lv_obj_align(s_wifi_status_label, LV_ALIGN_TOP_MID, 0, 10);
    
    s_wifi_ip_label = lv_label_create(scr);
    lv_label_set_text(s_wifi_ip_label,"");
    lv_obj_align(s_wifi_ip_label, LV_ALIGN_TOP_MID, 0, 30);


    s_retry_btn = lv_button_create(scr);
    lv_obj_set_size(s_retry_btn, 120, 50);
    lv_obj_align(s_retry_btn, LV_ALIGN_TOP_MID, 0, 65);
    lv_obj_add_event_cb(s_retry_btn, retry_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *retry_label = lv_label_create(s_retry_btn);
    lv_label_set_text(retry_label, "Retry");
    lv_obj_center(retry_label);

    // دکمه‌ی چراغ - جابه‌جا شد به سمت چپ مرکز تا جا برای Unlock باز شود
    s_light_btn = lv_button_create(scr);
    lv_obj_set_size(s_light_btn, 140, 70);
    lv_obj_align(s_light_btn, LV_ALIGN_CENTER, -80, 0);
    lv_obj_add_event_cb(s_light_btn, light_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_light_label = lv_label_create(s_light_btn);
    lv_label_set_text(s_light_label, "Light: OFF");
    lv_obj_center(s_light_label);

    // دکمه‌ی Unlock - کنار دکمه‌ی چراغ
    s_unlock_btn = lv_button_create(scr);
    lv_obj_set_size(s_unlock_btn, 140, 70);
    lv_obj_align(s_unlock_btn, LV_ALIGN_CENTER, 80, 0);
    lv_obj_set_style_bg_color(s_unlock_btn, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_add_event_cb(s_unlock_btn, unlock_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *unlock_label = lv_label_create(s_unlock_btn);
    lv_label_set_text(unlock_label, "Unlock");
    lv_obj_center(unlock_label);

    // دکمه‌ی کوچک تنظیمات - گوشه‌ی بالا-راست، دور از دسترس تصادفی
    s_settings_btn = lv_button_create(scr);
    lv_obj_set_size(s_settings_btn, 36, 36);
    lv_obj_align(s_settings_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_bg_color(s_settings_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_settings_btn, settings_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *settings_label = lv_label_create(s_settings_btn);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_label);

    s_sensor_label = lv_label_create(scr);
    lv_label_set_text(s_sensor_label, "Sensor: --");
    lv_obj_align(s_sensor_label, LV_ALIGN_BOTTOM_MID, 0, -10);


    s_qr_code = lv_qrcode_create(scr);
    lv_qrcode_set_size(s_qr_code, 90);
    lv_qrcode_set_dark_color(s_qr_code, lv_color_black());
    lv_qrcode_set_light_color(s_qr_code, lv_color_white());
    lv_obj_align(s_qr_code, LV_ALIGN_BOTTOM_RIGHT, -10, -50);
    lv_obj_add_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);   // تا وقتی IP مشخص نشده، مخفی بمونه
}

void ui_update_wifi_status(wifi_state_t state)
{
    if (s_wifi_status_label == NULL) {
        return;
    }
    lv_label_set_text(s_wifi_status_label, wifi_state_to_display_text(state));
}

void ui_update_wifi_ip(const char *ip_str)
{
    if (s_wifi_ip_label == NULL) {
        return;
    }
    if (ip_str == NULL || ip_str[0] == '\0') {
        lv_label_set_text(s_wifi_ip_label, "");
    } else {
        lv_label_set_text(s_wifi_ip_label, ip_str);
    }
}

void ui_update_light_status(bool on)
{
    if (s_light_label == NULL) {
        return;
    }
    lv_label_set_text(s_light_label, on ? "Light: ON" : "Light: OFF");
}

void ui_update_sensor_status(float temp, float hum, float pressure)
{
    if (s_sensor_label == NULL) {
        return;
    }
    char buf[64];
    snprintf(buf, sizeof(buf), "%.1f°C | %.0f%%RH | %.0fhPa", temp, hum, pressure);
    lv_label_set_text(s_sensor_label, buf);
}

void ui_update_capture_qr(const char *url)
{
    if (s_qr_code == NULL) {
        return;
    }
    if (url == NULL || url[0] == '\0') {
        lv_obj_add_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_qrcode_update(s_qr_code, url, strlen(url));
    lv_obj_clear_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
}