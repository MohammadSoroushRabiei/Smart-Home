#include "setting_screen.h"
#include "enroll_token.h"
#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "settings_screen";

static lv_obj_t *s_overlay;
static lv_obj_t *s_menu_view;
static lv_obj_t *s_enroll_view;
static lv_obj_t *s_qr_code;
static lv_obj_t *s_countdown_label;
static lv_timer_t *s_countdown_timer = NULL;
static int64_t s_token_expiry_us = 0;

// ---------------------------------------------------------------------
// جابه‌جایی بین دو نما
// ---------------------------------------------------------------------

static void show_menu_view(void)
{
    if (s_countdown_timer) {
        lv_timer_del(s_countdown_timer);
        s_countdown_timer = NULL;
    }
    lv_obj_add_flag(s_enroll_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_menu_view, LV_OBJ_FLAG_HIDDEN);
}

static void countdown_timer_cb(lv_timer_t *timer)
{
    int64_t remaining_us = s_token_expiry_us - esp_timer_get_time();

    if (remaining_us <= 0) {
        ESP_LOGI(TAG, "Enrollment token expired, returning to settings menu");
        show_menu_view();
        return;
    }

    int remaining_s = (int)(remaining_us / 1000000);
    char buf[32];
    snprintf(buf, sizeof(buf), "Expires in %d:%02d", remaining_s / 60, remaining_s % 60);
    lv_label_set_text(s_countdown_label, buf);
}

static void show_enroll_view(void)
{
    char token[ENROLL_TOKEN_LEN + 1];
    enroll_token_generate(token, sizeof(token));
    // زمان انقضا را همینجا محلی محاسبه می‌کنیم؛ چون از همان لحظه و همان
    // ثابت (ENROLL_TOKEN_VALID_MS) که خود ماژول enroll_token استفاده
    // می‌کند استفاده شده، هیچ‌وقت با انقضای واقعی توکن اختلاف پیدا نمی‌کند
    s_token_expiry_us = esp_timer_get_time() + ((int64_t)ENROLL_TOKEN_VALID_MS * 1000);

    char url[64];
    snprintf(url, sizeof(url), "https://%s/enroll?token=%s", wifi_get_ip_str(), token);
    lv_qrcode_update(s_qr_code, url, strlen(url));

    lv_obj_add_flag(s_menu_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_enroll_view, LV_OBJ_FLAG_HIDDEN);

    if (s_countdown_timer) {
        lv_timer_del(s_countdown_timer);
    }
    s_countdown_timer = lv_timer_create(countdown_timer_cb, 1000, NULL);
    countdown_timer_cb(NULL);   // نمایش فوری مقدار اولیه، بدون منتظر ماندن ۱ ثانیه
}

// ---------------------------------------------------------------------
// callback های دکمه‌ها
// ---------------------------------------------------------------------

static void enroll_btn_event_cb(lv_event_t *e)
{
    show_enroll_view();
}

static void enroll_back_btn_event_cb(lv_event_t *e)
{
    // انصراف صریح از QR enrollment - طبق تصمیم طراحی، توکن باطل می‌شود
    // و کاربر به منوی Settings برمی‌گردد (نه صفحه‌ی اصلی)
    enroll_token_invalidate();
    show_menu_view();
}

static void settings_back_btn_event_cb(lv_event_t *e)
{
    settings_screen_hide();
}

// ---------------------------------------------------------------------
// API عمومی
// ---------------------------------------------------------------------

void settings_screen_init(void)
{
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    // ===== نمای منو =====
    s_menu_view = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_menu_view);
    lv_obj_set_size(s_menu_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(s_menu_view, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back_btn = lv_button_create(s_menu_view);
    lv_obj_set_size(back_btn, 36, 36);
    lv_obj_align(back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(back_btn, settings_back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);

    lv_obj_t *title = lv_label_create(s_menu_view);
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_label_set_text(title, "Settings");
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // فعلاً فقط یک گزینه؛ فضای این منو برای آیتم‌های آینده (مدیریت چهره‌ها،
    // تغییر رمز از LCD، و غیره) در نظر گرفته شده
    lv_obj_t *enroll_btn = lv_button_create(s_menu_view);
    lv_obj_set_size(enroll_btn, 220, 60);
    lv_obj_align(enroll_btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(enroll_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(enroll_btn, enroll_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *enroll_label = lv_label_create(enroll_btn);
    lv_label_set_text(enroll_label, "Enroll New Face");
    lv_obj_center(enroll_label);

    // ===== نمای Enroll QR =====
    s_enroll_view = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_enroll_view);
    lv_obj_set_size(s_enroll_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(s_enroll_view, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *enroll_back_btn = lv_button_create(s_enroll_view);
    lv_obj_set_size(enroll_back_btn, 36, 36);
    lv_obj_align(enroll_back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(enroll_back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(enroll_back_btn, enroll_back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *enroll_back_label = lv_label_create(enroll_back_btn);
    lv_label_set_text(enroll_back_label, LV_SYMBOL_LEFT);
    lv_obj_center(enroll_back_label);

    lv_obj_t *enroll_title = lv_label_create(s_enroll_view);
    lv_obj_set_style_text_color(enroll_title, lv_color_white(), 0);
    lv_label_set_text(enroll_title, "Scan to Enroll");
    lv_obj_align(enroll_title, LV_ALIGN_TOP_MID, 0, 20);

    s_qr_code = lv_qrcode_create(s_enroll_view);
    lv_qrcode_set_size(s_qr_code, 160);
    lv_qrcode_set_dark_color(s_qr_code, lv_color_black());
    lv_qrcode_set_light_color(s_qr_code, lv_color_white());
    lv_obj_align(s_qr_code, LV_ALIGN_CENTER, 0, -10);

    s_countdown_label = lv_label_create(s_enroll_view);
    lv_obj_set_style_text_color(s_countdown_label, lv_color_white(), 0);
    lv_obj_set_width(s_countdown_label, 220);        
    lv_obj_set_style_text_align(s_countdown_label,LV_TEXT_ALIGN_CENTER,0); 
    lv_label_set_text(s_countdown_label, "");
    lv_obj_align_to(s_countdown_label, s_qr_code, LV_ALIGN_OUT_BOTTOM_MID, 0,15);
    lv_obj_add_flag(s_enroll_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Settings screen initialized");
}

void settings_screen_show(void)
{
    show_menu_view();
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void settings_screen_hide(void)
{
    if (s_countdown_timer) {
        lv_timer_del(s_countdown_timer);
        s_countdown_timer = NULL;
    }
    // اگر کاربر وسط QR enrollment از کل Settings خارج شود (نه فقط از نمای
    // QR)، توکن فعال (در صورت وجود) هم باید باطل شود - فراخوانی این تابع
    // حتی وقتی توکنی فعال نیست کاملاً بی‌خطر است
    enroll_token_invalidate();

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}