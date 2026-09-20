#include "attendance_screen.h"
#include "keypad_screen.h"
#include "enroll_token.h"
#include "attendance.h"
#include "wifi_manager.h"

#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "attendance_screen";

static lv_obj_t *s_overlay;
static lv_obj_t *s_title;
static lv_obj_t *s_qr;
static lv_obj_t *s_countdown;
static lv_obj_t *s_hint;          // پیام وقتی سرور کانفیگ نشده است
static lv_obj_t *s_enroll_btn;
static lv_obj_t *s_enroll_label;
static lv_timer_t *s_timer = NULL;
static int64_t s_expiry_us = 0;
static bool s_mode_enroll = false;   // false = QR حضور، true = QR ثبت‌نام
static bool s_open = false;

// ---------------------------------------------------------------------
// QR و زمان‌سنج
// ---------------------------------------------------------------------

// ساخت/تازه‌سازی توکن و QR مطابق حالت فعلی (حضور یا ثبت‌نام)
static void refresh_qr(void)
{
    char token[ENROLL_TOKEN_LEN + 1];
    char url[80];

    if (s_mode_enroll) {
        enroll_token_generate(ENROLL_TOKEN_PURPOSE_ENROLL, token, sizeof(token));
        snprintf(url, sizeof(url), "https://%s/enroll?token=%s", wifi_get_ip_str(), token);
        s_expiry_us = esp_timer_get_time() + ((int64_t)ENROLL_TOKEN_VALID_MS * 1000);
        lv_label_set_text(s_title, "Scan to Enroll");
        lv_label_set_text(s_enroll_label, "Attendance QR");
    } else {
        enroll_token_generate(ENROLL_TOKEN_PURPOSE_ATTENDANCE, token, sizeof(token));
        snprintf(url, sizeof(url), "https://%s/attendance?token=%s", wifi_get_ip_str(), token);
        s_expiry_us = esp_timer_get_time() + ((int64_t)ATTEND_TOKEN_VALID_MS * 1000);
        lv_label_set_text(s_title, "Scan to Record");
        lv_label_set_text(s_enroll_label, "Enroll New Face");
    }
    lv_qrcode_update(s_qr, url, strlen(url));
}

static void countdown_timer_cb(lv_timer_t *timer)
{
    // در حالت حضورِ غیرفعال (سرور کانفیگ نشده) زمان‌سنجی برای تازه‌سازی نیست
    if (!s_mode_enroll && !attendance_is_enabled()) {
        return;
    }

    int64_t remaining_us = s_expiry_us - esp_timer_get_time();
    if (remaining_us <= 0) {
        // برخلاف نمای enroll قدیمی، صفحه بسته نمی‌شود - توکن تازه و QR جدید
        // (توکن قبلی هم تا پایان مهلت خودش معتبر می‌ماند - گریس ماژول توکن)
        ESP_LOGI(TAG, "Token expired - regenerating QR");
        refresh_qr();
        return;
    }

    int remaining_s = (int)(remaining_us / 1000000);
    char buf[32];
    snprintf(buf, sizeof(buf), "Expires in %d:%02d", remaining_s / 60, remaining_s % 60);
    lv_label_set_text(s_countdown, buf);
}

// ---------------------------------------------------------------------
// دکمه‌ها
// ---------------------------------------------------------------------

static void enroll_password_result(keypad_purpose_t purpose, bool success);

static void back_btn_event_cb(lv_event_t *e)
{
    attendance_screen_hide();
}

static void enroll_btn_event_cb(lv_event_t *e)
{
    if (s_mode_enroll) {
        // برگشت به QR حضور
        s_mode_enroll = false;
        refresh_qr();
        return;
    }

    // ثبت/ویرایش افراد پشت رمز تنظیمات است؛ با تایید رمز، کیپد همین تابع
    // را صدا می‌زند و ما به QR ثبت‌نام سوییچ می‌کنیم
    keypad_screen_show(KEYPAD_PURPOSE_ENROLL, enroll_password_result);
}

// callback نتیجه‌ی کیپد - با رمز درست به QR ثبت‌نام سوییچ می‌کنیم
static void enroll_password_result(keypad_purpose_t purpose, bool success)
{
    if (!success) {
        return;   // رمز غلط - در همان کیپد پیام خطا دیده می‌شود
    }
    if (!s_open) {
        // کاربر بعد از وارد کردن رمز، صفحه را بسته - کاری نکن
        return;
    }
    s_mode_enroll = true;
    refresh_qr();
}

// ---------------------------------------------------------------------
// ساخت و API عمومی
// ---------------------------------------------------------------------

void attendance_screen_init(void)
{
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

    s_title = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_title, lv_color_white(), 0);
    lv_label_set_text(s_title, "Attendance");
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, 20);

    s_qr = lv_qrcode_create(s_overlay);
    lv_qrcode_set_size(s_qr, 170);
    lv_qrcode_set_dark_color(s_qr, lv_color_black());
    lv_qrcode_set_light_color(s_qr, lv_color_white());
    lv_obj_align(s_qr, LV_ALIGN_CENTER, 0, -40);

    s_countdown = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_countdown, lv_color_white(), 0);
    lv_obj_set_width(s_countdown, 220);
    lv_obj_set_style_text_align(s_countdown, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_countdown, "");
    lv_obj_align_to(s_countdown, s_qr, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    s_hint = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_hint, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_width(s_hint, 280);
    lv_obj_set_style_text_align(s_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_hint, "Attendance is not configured.\n"
                      "Set the local server URL in the web\n"
                      "dashboard under System Settings.");
    lv_obj_align(s_hint, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);

    s_enroll_btn = lv_button_create(s_overlay);
    lv_obj_set_size(s_enroll_btn, 220, 50);
    lv_obj_align(s_enroll_btn, LV_ALIGN_BOTTOM_MID, 0, -25);
    lv_obj_set_style_bg_color(s_enroll_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(s_enroll_btn, enroll_btn_event_cb, LV_EVENT_CLICKED, NULL);
    s_enroll_label = lv_label_create(s_enroll_btn);
    lv_label_set_text(s_enroll_label, "Enroll New Face");
    lv_obj_center(s_enroll_label);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Attendance screen initialized");
}

void attendance_screen_show(void)
{
    s_open = true;
    s_mode_enroll = false;

    // تضمین اینکه روی هر overlay دیگری (تنظیمات، کیپد و ...) قرار بگیرد
    lv_obj_move_foreground(s_overlay);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    if (attendance_is_enabled()) {
        lv_obj_clear_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_countdown, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        refresh_qr();
    } else {
        lv_obj_add_flag(s_qr, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_countdown, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_hint, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_title, "Attendance");
        lv_label_set_text(s_enroll_label, "Enroll New Face");
    }

    if (s_timer) {
        lv_timer_del(s_timer);
    }
    s_timer = lv_timer_create(countdown_timer_cb, 1000, NULL);
    countdown_timer_cb(NULL);
}

void attendance_screen_hide(void)
{
    if (s_timer) {
        lv_timer_del(s_timer);
        s_timer = NULL;
    }
    // بستن صفحه یعنی هیچ QR ای زنده نمی‌ماند - هر دو منظور باطل
    enroll_token_invalidate(ENROLL_TOKEN_PURPOSE_ATTENDANCE);
    enroll_token_invalidate(ENROLL_TOKEN_PURPOSE_ENROLL);

    s_open = false;
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

bool attendance_screen_is_open(void)
{
    return s_open;
}
