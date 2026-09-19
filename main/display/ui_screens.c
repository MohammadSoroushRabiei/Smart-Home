#include "ui_screens.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lcd_driver.h"
#include "wifi_manager.h"
#include "wifi_setup_screen.h"
#include "app_state.h"
#include "keypad_screen.h"
#include "esp_log.h"
#include "mqtt_manager.h"
#include "mqtt_setup_screen.h"
#include "setting_screen.h"
#include "ml_agent.h"

static const char *TAG = "ui_screens";

static lv_obj_t *s_wifi_btn;
static lv_obj_t *s_wifi_label;
static lv_obj_t *s_wifi_ip_label;
static lv_obj_t *s_light_btn;
static lv_obj_t *s_light_label;
static lv_obj_t *s_unlock_btn;
static lv_obj_t *s_unlock_label;
static lv_obj_t *s_settings_btn;

// ===== ردیف دوم: دکمه‌ی فن + دکمه‌ی حالت ML (همگام با HA) =====
static lv_obj_t *s_fan_btn;
static lv_obj_t *s_fan_label;
static lv_obj_t *s_ml_btn;
static lv_obj_t *s_ml_btn_label;

// پاپ‌آپ جزئیات ML - با هولد روی دکمه‌ی ML باز می‌شود
static lv_obj_t *s_ml_popup;
static lv_obj_t *s_ml_pop_mode;     // "Mode: SHADOW"
static lv_obj_t *s_ml_pop_acc;      // "Agrees: Light 87% Fan 92%"
static lv_obj_t *s_ml_pop_light;    // "Light should be: ON (95%)"
static lv_obj_t *s_ml_pop_fan;      // "Fan should be: OFF (10%)"
static lv_obj_t *s_ml_pop_env;      // "Presence: Home • 45 lx"
static bool s_ml_btn_hold_handled = false;
static bool s_ml_popup_visible = false;

// ===== کارت‌های سنسور (جایگزین لیبل واحد قبلی) =====
static lv_obj_t *s_temp_value_label;
static lv_obj_t *s_hum_value_label;
static lv_obj_t *s_press_value_label;
static lv_obj_t *s_mqtt_btn;

// QR داشبورد وب زیر لیبل IP — با اتصال WiFi پر و با قطع آن پنهان می‌شود
static lv_obj_t *s_dash_qr;

// بعد از یک هولد روی دکمه‌ی WiFi، LVGL معمولاً یک CLICKED اضافه هم موقع
// رهاکردن انگشت می‌فرستد - این فلگ از اجرای اشتباه منطق تپ جلوگیری می‌کند.
static bool s_wifi_long_press_handled = false;
static bool s_mqtt_long_press_handled = false;
static volatile bool s_wifi_off_in_progress = false;


static void light_btn_event_cb(lv_event_t *e)
{
    app_state_set_light(!app_state_get_light());
}

static void fan_btn_event_cb(lv_event_t *e)
{
    // از مسیر رسمی app_state می‌رود: مثل چراغ، نمونه‌ی آموزشی برای مدل ثبت می‌شود
    app_state_set_fan(!app_state_get_fan());
}

static void ml_popup_close(void)
{
    if (s_ml_popup != NULL) {
        lv_obj_add_flag(s_ml_popup, LV_OBJ_FLAG_HIDDEN);
        s_ml_popup_visible = false;
    }
}

// تپ روی دکمه‌ی ML = تغییر حالت autonomy (همان سوییچ ML Autonomy در HA؛
// هر دو طرف فوراً همگام می‌شوند)
static void ml_btn_click_cb(lv_event_t *e)
{
    if (s_ml_btn_hold_handled) {
        s_ml_btn_hold_handled = false;
        return;
    }
    ml_agent_set_autonomy(!ml_agent_any_auto());
}

// هولد روی دکمه‌ی ML = پاپ‌آپ جزئیات (دقت، پیش‌بینی، محیط)
static void ml_btn_hold_cb(lv_event_t *e)
{
    s_ml_btn_hold_handled = true;
    if (s_ml_popup != NULL) {
        lv_obj_clear_flag(s_ml_popup, LV_OBJ_FLAG_HIDDEN);
        s_ml_popup_visible = true;
    }
}

static void ml_popup_close_cb(lv_event_t *e)
{
    (void)e;
    ml_popup_close();
}

static void on_keypad_result(keypad_purpose_t purpose, bool success)
{
    if (purpose == KEYPAD_PURPOSE_UNLOCK) {
        mqtt_manager_publish_access_event(success ? ACCESS_EVENT_GRANTED_CODE : ACCESS_EVENT_DENIED_CODE);
        if (success) {
            ESP_LOGI(TAG, "Unlock code correct - ACCESS GRANTED");
            app_state_set_lock(true);
        } else {
            ESP_LOGW(TAG, "Unlock code incorrect - ACCESS DENIED");
        }
    } else if (purpose == KEYPAD_PURPOSE_SETTINGS) {
        if (success) {
            ESP_LOGI(TAG, "Settings code correct");
            settings_screen_show();
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

// ترتیب مهم است: اول MQTT (publish "offline" + stop)، بعد رادیو Wi-Fi.
// publish/stop بلاک‌کننده‌اند (تا ~۱ ثانیه) پس نباید در تسک LVGL اجرا شوند.
static void wifi_off_task(void *arg)
{
    mqtt_manager_prepare_for_network_loss();
    vTaskDelay(pdMS_TO_TICKS(150));   // فرصت خالی‌شدن بافر TCP قبل از خاموش‌شدن رادیو
    wifi_manager_disable();
    s_wifi_off_in_progress = false;
    vTaskDelete(NULL);
}

static void wifi_btn_click_cb(lv_event_t *e)
{
    if (s_wifi_long_press_handled) {
        s_wifi_long_press_handled = false;
        return;
    }

    wifi_state_t state = wifi_get_state();
    if (state == WIFI_STATE_CONNECTED) {
        if (s_wifi_off_in_progress) {
            return;   // یک قطع در جریان است - تپ دوباره را نادیده بگیر
        }
        s_wifi_off_in_progress = true;
        if (xTaskCreate(wifi_off_task, "wifi_off", 4096, NULL, 3, NULL) != pdPASS) {
            s_wifi_off_in_progress = false;
            wifi_manager_disable();   // fallback: بدون publish آفلاین
        }
    } else if (state == WIFI_STATE_OFFLINE) {
        wifi_manager_enable();
    }
}

static void wifi_btn_hold_cb(lv_event_t *e)
{
    s_wifi_long_press_handled = true;
    wifi_setup_screen_show();
}


static lv_obj_t *create_sensor_card(lv_obj_t *parent, const char *title, const char *unit,
                                     lv_color_t accent_color, lv_obj_t **out_value_label)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_remove_style_all(card);
    lv_obj_set_size(card, 96, 96);
    lv_obj_set_style_bg_color(card, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(card, 14, 0);
    lv_obj_set_style_border_width(card, 3, 0);
    lv_obj_set_style_border_color(card, accent_color, 0);
    lv_obj_set_style_border_side(card, LV_BORDER_SIDE_TOP, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title_label = lv_label_create(card);
    lv_obj_set_style_text_color(title_label, lv_color_hex(0x999999), 0);
    lv_label_set_text(title_label, title);
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 10);

    lv_obj_t *value_label = lv_label_create(card);
    lv_obj_set_style_text_color(value_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(value_label, &lv_font_montserrat_28, 0);
    lv_label_set_text(value_label, "--");
    // اگر واحد داریم، مقدار کمی بالاتر از مرکز قرار می‌گیرد تا جا برای واحد زیرش باز شود
    lv_obj_align(value_label, LV_ALIGN_CENTER, 0, (unit != NULL && unit[0] != '\0') ? 2 : 10);

    if (unit != NULL && unit[0] != '\0') {
        lv_obj_t *unit_label = lv_label_create(card);
        lv_obj_set_style_text_color(unit_label, lv_color_hex(0x999999), 0);
        lv_label_set_text(unit_label, unit);
        lv_obj_align_to(unit_label, value_label, LV_ALIGN_OUT_BOTTOM_MID, 0, 2);
    }

    *out_value_label = value_label;
    return card;
}

// دکمه‌ی MQTT/HA به‌صورت یک toggle واقعی عمل می‌کند: تپ وقتی خاموش است
// (UNCONFIGURED/DISABLED) روشنش می‌کند (یا تنظیمات را باز می‌کند اگر هنوز
// چیزی تنظیم نشده)؛ تپ وقتی روشن است (CONNECTING/CONNECTED) خاموشش می‌کند.
static void mqtt_btn_click_cb(lv_event_t *e)
{
    if (s_mqtt_long_press_handled) {
        s_mqtt_long_press_handled = false;
        return;
    }

    mqtt_manager_state_t state = mqtt_manager_get_state();
    switch (state) {
        case MQTT_MGR_STATE_UNCONFIGURED:
            mqtt_setup_screen_show();
            break;
        case MQTT_MGR_STATE_DISABLED:
            mqtt_manager_enable();
            break;
        case MQTT_MGR_STATE_CONNECTING:
        case MQTT_MGR_STATE_CONNECTED:
        default:
            mqtt_manager_disable();
            break;
    }
}

static void mqtt_btn_hold_cb(lv_event_t *e)
{
    s_mqtt_long_press_handled = true;
    mqtt_setup_screen_show();
}

void ui_screens_init(void)
{
    lv_obj_t *scr = lv_screen_active();

    s_mqtt_btn = lv_button_create(scr);
    lv_obj_set_size(s_mqtt_btn, 40, 40);
    lv_obj_align(s_mqtt_btn, LV_ALIGN_TOP_LEFT, 10, 10);
    lv_obj_set_style_bg_color(s_mqtt_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_mqtt_btn, mqtt_btn_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_mqtt_btn, mqtt_btn_hold_cb, LV_EVENT_LONG_PRESSED, NULL);

    lv_obj_t *mqtt_icon = lv_label_create(s_mqtt_btn);
    lv_label_set_text(mqtt_icon, LV_SYMBOL_HOME);
    lv_obj_center(mqtt_icon);

    s_wifi_btn = lv_button_create(scr);
    lv_obj_set_size(s_wifi_btn, 130, 45);
    lv_obj_align(s_wifi_btn, LV_ALIGN_TOP_MID, 0, 10);
    lv_obj_set_style_bg_color(s_wifi_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_wifi_btn, wifi_btn_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_wifi_btn, wifi_btn_hold_cb, LV_EVENT_LONG_PRESSED, NULL);

    s_wifi_label = lv_label_create(s_wifi_btn);
    lv_label_set_text(s_wifi_label, LV_SYMBOL_WIFI " WiFi");
    lv_obj_center(s_wifi_label);

    s_wifi_ip_label = lv_label_create(scr);
    lv_obj_set_style_text_align(s_wifi_ip_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_wifi_ip_label, "Offline");
    lv_obj_align(s_wifi_ip_label, LV_ALIGN_TOP_MID, 0, 62);

    // QR داشبورد وب — زیر لیبل IP؛ فقط وقتی WiFi وصل است نشان داده می‌شود.
    // پهنای آزاد بین لیبل IP (تا ~y=96) و ردیف دکمه‌ها (از ~y=205) است.
    s_dash_qr = lv_qrcode_create(scr);
    lv_qrcode_set_size(s_dash_qr, 90);
    lv_obj_align(s_dash_qr, LV_ALIGN_TOP_MID, 0, 98);
    lv_obj_add_flag(s_dash_qr, LV_OBJ_FLAG_HIDDEN);

    s_light_btn = lv_button_create(scr);
    lv_obj_set_size(s_light_btn, 140, 70);
    lv_obj_align(s_light_btn, LV_ALIGN_CENTER, -80, 0);
    lv_obj_set_style_bg_color(s_light_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_light_btn, light_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_light_label = lv_label_create(s_light_btn);
    lv_label_set_text(s_light_label, LV_SYMBOL_POWER " Light: OFF");
    lv_obj_center(s_light_label);

    s_unlock_btn = lv_button_create(scr);
    lv_obj_set_size(s_unlock_btn, 140, 70);
    lv_obj_align(s_unlock_btn, LV_ALIGN_CENTER, 80, 0);
    lv_obj_set_style_bg_color(s_unlock_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_unlock_btn, unlock_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_unlock_label = lv_label_create(s_unlock_btn);
    lv_label_set_text(s_unlock_label, LV_SYMBOL_CLOSE " Locked");
    lv_obj_center(s_unlock_label);

    // ===== ردیف دوم: فن + دکمه‌ی حالت ML =====
    s_fan_btn = lv_button_create(scr);
    lv_obj_set_size(s_fan_btn, 140, 70);
    lv_obj_align(s_fan_btn, LV_ALIGN_CENTER, -80, 85);
    lv_obj_set_style_bg_color(s_fan_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_fan_btn, fan_btn_event_cb, LV_EVENT_CLICKED, NULL);

    s_fan_label = lv_label_create(s_fan_btn);
    lv_label_set_text(s_fan_label, LV_SYMBOL_STOP " Fan: OFF");
    lv_obj_center(s_fan_label);

    // تپ = تغییر SHADOW/AUTO، هولد = پاپ‌آپ جزئیات
    s_ml_btn = lv_button_create(scr);
    lv_obj_set_size(s_ml_btn, 140, 70);
    lv_obj_align(s_ml_btn, LV_ALIGN_CENTER, 80, 85);
    lv_obj_set_style_bg_color(s_ml_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_ml_btn, ml_btn_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(s_ml_btn, ml_btn_hold_cb, LV_EVENT_LONG_PRESSED, NULL);

    s_ml_btn_label = lv_label_create(s_ml_btn);
    lv_label_set_text(s_ml_btn_label, LV_SYMBOL_EYE_OPEN " ML: SHADOW");
    lv_obj_center(s_ml_btn_label);

    s_settings_btn = lv_button_create(scr);
    lv_obj_set_size(s_settings_btn, 40, 40);
    lv_obj_align(s_settings_btn, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_bg_color(s_settings_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_settings_btn, settings_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *settings_label = lv_label_create(s_settings_btn);
    lv_label_set_text(settings_label, LV_SYMBOL_SETTINGS);
    lv_obj_center(settings_label);

    lv_obj_t *card_temp = create_sensor_card(scr, "Temp", NULL,
        lv_palette_main(LV_PALETTE_ORANGE), &s_temp_value_label);
    lv_obj_align(card_temp, LV_ALIGN_BOTTOM_MID, -104, -14);

    lv_obj_t *card_hum = create_sensor_card(scr, "Humidity", NULL,
        lv_palette_main(LV_PALETTE_BLUE), &s_hum_value_label);
    lv_obj_align(card_hum, LV_ALIGN_BOTTOM_MID, 0, -14);

    lv_obj_t *card_press = create_sensor_card(scr, "Pressure", "hPa",
        lv_palette_main(LV_PALETTE_GREEN), &s_press_value_label);
    lv_obj_align(card_press, LV_ALIGN_BOTTOM_MID, 104, -14);

    // ===== پاپ‌آپ جزئیات ML (هولد روی دکمه‌ی ML) - آخرین ویجت تا روی همه باشد =====
    // پس‌زمینه‌ی نیمه‌شفاف: تپ روی فضای خالی = بستن
    s_ml_popup = lv_obj_create(scr);
    lv_obj_remove_style_all(s_ml_popup);
    lv_obj_set_size(s_ml_popup, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_ml_popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_ml_popup, LV_OPA_60, 0);
    lv_obj_clear_flag(s_ml_popup, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_ml_popup, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(s_ml_popup, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_ml_popup, ml_popup_close_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *panel = lv_obj_create(s_ml_popup);
    lv_obj_remove_style_all(panel);
    lv_obj_set_size(panel, 250, 200);
    lv_obj_align(panel, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_radius(panel, 14, 0);
    lv_obj_set_style_border_width(panel, 3, 0);
    lv_obj_set_style_border_color(panel, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pop_title = lv_label_create(panel);
    lv_label_set_text(pop_title, "ML Agent");
    lv_obj_set_style_text_color(pop_title, lv_color_white(), 0);
    lv_obj_align(pop_title, LV_ALIGN_TOP_LEFT, 12, 10);

    lv_obj_t *pop_close = lv_button_create(panel);
    lv_obj_set_size(pop_close, 34, 34);
    lv_obj_align(pop_close, LV_ALIGN_TOP_RIGHT, -8, -8);
    lv_obj_add_event_cb(pop_close, ml_popup_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *close_icon = lv_label_create(pop_close);
    lv_label_set_text(close_icon, LV_SYMBOL_CLOSE);
    lv_obj_center(close_icon);

    s_ml_pop_mode = lv_label_create(panel);
    lv_obj_set_style_text_color(s_ml_pop_mode, lv_color_white(), 0);
    lv_label_set_text(s_ml_pop_mode, "Mode: SHADOW");
    lv_obj_align(s_ml_pop_mode, LV_ALIGN_TOP_LEFT, 12, 40);

    s_ml_pop_acc = lv_label_create(panel);
    lv_obj_set_style_text_color(s_ml_pop_acc, lv_color_white(), 0);
    lv_label_set_text(s_ml_pop_acc, "Agrees: Light --% Fan --%");
    lv_obj_align(s_ml_pop_acc, LV_ALIGN_TOP_LEFT, 12, 62);

    s_ml_pop_light = lv_label_create(panel);
    lv_obj_set_style_text_color(s_ml_pop_light, lv_color_white(), 0);
    lv_label_set_text(s_ml_pop_light, "Light should be: -- (--%)");
    lv_obj_align(s_ml_pop_light, LV_ALIGN_TOP_LEFT, 12, 84);

    s_ml_pop_fan = lv_label_create(panel);
    lv_obj_set_style_text_color(s_ml_pop_fan, lv_color_white(), 0);
    lv_label_set_text(s_ml_pop_fan, "Fan should be: -- (--%)");
    lv_obj_align(s_ml_pop_fan, LV_ALIGN_TOP_LEFT, 12, 106);

    s_ml_pop_env = lv_label_create(panel);
    lv_obj_set_style_text_color(s_ml_pop_env, lv_color_white(), 0);
    lv_label_set_text(s_ml_pop_env, "Presence: -- • -- lx");
    lv_obj_align(s_ml_pop_env, LV_ALIGN_TOP_LEFT, 12, 128);

    lv_obj_t *pop_hint = lv_label_create(panel);
    lv_obj_set_style_text_color(pop_hint, lv_color_hex(0x999999), 0);
    lv_label_set_text(pop_hint, "Tap ML button = switch mode");
    lv_obj_align(pop_hint, LV_ALIGN_BOTTOM_LEFT, 12, -10);
}

void ui_update_wifi_status(wifi_state_t state)
{
    if (s_wifi_btn == NULL || s_wifi_label == NULL || s_wifi_ip_label == NULL) {
        return;
    }

    lv_color_t color;
    char status_text[64];

    switch (state) {
        case WIFI_STATE_CONNECTED: {
            color = lv_palette_main(LV_PALETTE_BLUE);
            snprintf(status_text, sizeof(status_text), "Connected to: %s\n%s",
                     wifi_get_connected_ssid(), wifi_get_ip_str());
            break;
        }
        case WIFI_STATE_CONNECTING:
            color = lv_palette_main(LV_PALETTE_ORANGE);
            snprintf(status_text, sizeof(status_text), "Connecting...");
            break;
        case WIFI_STATE_OFFLINE:
        default:
            color = lv_palette_main(LV_PALETTE_GREY);
            snprintf(status_text, sizeof(status_text), "Offline");
            break;
    }

    lv_obj_set_style_bg_color(s_wifi_btn, color, 0);
    lv_label_set_text(s_wifi_ip_label, status_text);
}

void ui_update_light_status(bool on)
{
    if (s_light_label == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(s_light_btn,
        on ? lv_palette_main(LV_PALETTE_ORANGE) : lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_light_label, on ? LV_SYMBOL_CHARGE " Light: ON"
                                        : LV_SYMBOL_POWER " Light: OFF");
}

void ui_update_lock_status(bool unlocked)
{
    if (s_unlock_btn == NULL || s_unlock_label == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(s_unlock_btn,
        unlocked ? lv_palette_main(LV_PALETTE_GREEN) : lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_unlock_label, unlocked ? LV_SYMBOL_OK " Unlocked"
                                               : LV_SYMBOL_CLOSE " Locked");
}

void ui_update_fan_status(bool on)
{
    if (s_fan_label == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(s_fan_btn,
        on ? lv_palette_main(LV_PALETTE_CYAN) : lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_fan_label, on ? LV_SYMBOL_REFRESH " Fan: ON"
                                      : LV_SYMBOL_STOP " Fan: OFF");
}

void ui_update_ml_status(bool any_auto, float acc_light, uint32_t n_light,
                         float acc_fan, uint32_t n_fan,
                         float p_light, float p_fan)
{
    if (s_ml_btn == NULL || s_ml_btn_label == NULL) {
        return;
    }

    // دکمه: خاکستری = سایه (فقط تماشا)، آبی = خودکار
    lv_obj_set_style_bg_color(s_ml_btn,
        any_auto ? lv_palette_main(LV_PALETTE_BLUE) : lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_ml_btn_label, any_auto ? LV_SYMBOL_PLAY " ML: AUTO"
                                               : LV_SYMBOL_EYE_OPEN " ML: SHADOW");

    if (!s_ml_popup_visible || s_ml_pop_mode == NULL) {
        return;
    }

    char buf[40];
    snprintf(buf, sizeof(buf), "Mode: %s", any_auto ? "AUTO" : "SHADOW");
    lv_label_set_text(s_ml_pop_mode, buf);

    if (n_light > 0 || n_fan > 0) {
        snprintf(buf, sizeof(buf), "Agrees: Light %.0f%% Fan %.0f%%", acc_light, acc_fan);
    } else {
        snprintf(buf, sizeof(buf), "Agrees: Light -- Fan --");
    }
    lv_label_set_text(s_ml_pop_acc, buf);

    // اطمینان مدل به همان سمتی که می‌گوید: max(p, 1-p)
    snprintf(buf, sizeof(buf), "Light should be: %s (%.0f%%)",
             p_light >= 0.5f ? "ON" : "OFF",
             (p_light >= 0.5f ? p_light : 1.0f - p_light) * 100.0f);
    lv_label_set_text(s_ml_pop_light, buf);

    snprintf(buf, sizeof(buf), "Fan should be: %s (%.0f%%)",
             p_fan >= 0.5f ? "ON" : "OFF",
             (p_fan >= 0.5f ? p_fan : 1.0f - p_fan) * 100.0f);
    lv_label_set_text(s_ml_pop_fan, buf);
}

void ui_update_virtual_env(bool presence, float lux)
{
    if (s_ml_pop_env == NULL) {
        return;
    }
    const char *room = lux < 10.0f  ? "Dark"
                     : lux < 50.0f  ? "Dim"
                     : lux < 300.0f ? "Indoor"
                                    : "Bright";
    char buf[64];
    snprintf(buf, sizeof(buf), "In Home: %s\nRoom luminance: %s (%.0f lx)",
             presence ? "Yes" : "No", room, lux);
    lv_label_set_text(s_ml_pop_env, buf);
}

void ui_update_dashboard_qr(const char *url)
{
    if (s_dash_qr == NULL) {
        return;
    }

    if (url == NULL || url[0] == '\0') {
        lv_obj_add_flag(s_dash_qr, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_qrcode_update(s_dash_qr, url, strlen(url));
    lv_obj_clear_flag(s_dash_qr, LV_OBJ_FLAG_HIDDEN);
}

void ui_update_sensor_status(float temp, float hum, float pressure)
{
    if (s_temp_value_label == NULL || s_hum_value_label == NULL || s_press_value_label == NULL) {
        return;
    }

    char buf[16];

    snprintf(buf, sizeof(buf), "%.1f°", temp);
    lv_label_set_text(s_temp_value_label, buf);

    snprintf(buf, sizeof(buf), "%.0f%%", hum);
    lv_label_set_text(s_hum_value_label, buf);

    snprintf(buf, sizeof(buf), "%.0f", pressure);
    lv_label_set_text(s_press_value_label, buf);
}

void ui_update_mqtt_status(mqtt_manager_state_t state)
{
    if (s_mqtt_btn == NULL) {
        return;
    }

    lv_color_t color;
    switch (state) {
        case MQTT_MGR_STATE_CONNECTED:
            color = lv_palette_main(LV_PALETTE_BLUE);
            break;
        case MQTT_MGR_STATE_CONNECTING:
            color = lv_palette_main(LV_PALETTE_ORANGE);
            break;
        case MQTT_MGR_STATE_DISABLED:
        case MQTT_MGR_STATE_UNCONFIGURED:
        default:
            color = lv_palette_main(LV_PALETTE_GREY);
            break;
    }
    lv_obj_set_style_bg_color(s_mqtt_btn, color, 0);
}
