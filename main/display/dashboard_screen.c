#include "dashboard_screen.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "wifi_manager.h"
#include "esp_log.h"

static const char *TAG = "dashboard_screen";

static lv_obj_t *s_overlay;
static lv_obj_t *s_qr;
static lv_obj_t *s_url_label;      // آدرس داشبورد زیر QR - تنها متن صفحه
static lv_obj_t *s_offline_hint;   // پیام وقتی WiFi وصل نیست
static bool s_open = false;
static char s_url[40];             // آخرین URL رسیده از main - خالی = آفلاین

static void back_btn_event_cb(lv_event_t *e)
{
    (void)e;
    dashboard_screen_hide();
}

static void refresh(void)
{
    // اگر main هنوز URL نفرستاده ولی رادیو وصل است، خودمان از IP فعلی می‌سازیم
    if (s_url[0] == '\0' && wifi_get_state() == WIFI_STATE_CONNECTED) {
        snprintf(s_url, sizeof(s_url), "https://%s/", wifi_get_ip_str());
    }

    if (s_url[0] != '\0') {
        lv_qrcode_update(s_qr, s_url, strlen(s_url));
        // نمایش بدون اسلش انتهایی: فقط https://<IP>
        char disp[40];
        snprintf(disp, sizeof(disp), "%s", s_url);
        size_t n = strlen(disp);
        if (n > 0 && disp[n - 1] == '/') {
            disp[n - 1] = '\0';
        }
        lv_label_set_text(s_url_label, disp);
        lv_obj_clear_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_url_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_offline_hint, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_url_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_offline_hint, LV_OBJ_FLAG_HIDDEN);
    }
}

void dashboard_screen_init(void)
{
    s_url[0] = '\0';

    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    lv_obj_t *back_btn = lv_button_create(s_overlay);
    lv_obj_set_size(back_btn, 36, 36);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Web Dashboard");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    s_qr = lv_qrcode_create(s_overlay);
    lv_qrcode_set_size(s_qr, 170);
    lv_qrcode_set_dark_color(s_qr, lv_color_black());
    lv_qrcode_set_light_color(s_qr, lv_color_white());
    lv_obj_align(s_qr, LV_ALIGN_CENTER, 0, -60);

    // زیر QR فقط آدرس می‌آید. عرض ثابت + تراز TOP_MID (نه align_to) تا با
    // تغییر متن، لیبل وسط صفحه بماند - align_to موقعیت را یک‌بار برای متن
    // اولیه حساب می‌کند و لیبل بلندتر به سمت راست می‌چسبد
    s_url_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_url_label, lv_color_white(), 0);
    lv_obj_set_width(s_url_label, 300);
    lv_obj_set_style_text_align(s_url_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_url_label, "");
    lv_obj_align(s_url_label, LV_ALIGN_TOP_MID, 0, 285);

    s_offline_hint = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_offline_hint, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_width(s_offline_hint, 280);
    lv_obj_set_style_text_align(s_offline_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_offline_hint, "WiFi is not connected.\n"
                      "Connect to WiFi to see the\n"
                      "dashboard address here.");
    lv_obj_align(s_offline_hint, LV_ALIGN_CENTER, 0, -40);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Dashboard screen initialized");
}

void dashboard_screen_show(void)
{
    s_open = true;

    // تضمین اینکه روی هر overlay دیگری (تنظیمات، کیپد و ...) قرار بگیرد
    lv_obj_move_foreground(s_overlay);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    refresh();
}

void dashboard_screen_hide(void)
{
    s_open = false;
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

bool dashboard_screen_is_open(void)
{
    return s_open;
}

void dashboard_screen_set_url(const char *url)
{
    if (url != NULL && url[0] != '\0') {
        snprintf(s_url, sizeof(s_url), "%s", url);
    } else {
        s_url[0] = '\0';
    }

    if (s_open) {
        refresh();
    }
}
