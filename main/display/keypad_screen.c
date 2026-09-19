#include "keypad_screen.h"
#include "password_manager.h"
#include "lcd_driver.h"

#include <string.h>
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "keypad_screen";

// نقشه‌ی دکمه‌های کیبورد (LV_BUTTONMATRIX_CTRL بعداً برای رنگ دکمه‌ی OK ست می‌شود)
static const char *s_btnm_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    "C", "0", "OK", ""
};

static lv_obj_t *s_overlay;
static lv_obj_t *s_title_label;
static lv_obj_t *s_qr_code;
static lv_obj_t *s_qr_label;
static lv_obj_t *s_textarea;
static lv_obj_t *s_eye_btn;
static lv_obj_t *s_error_label;
static lv_obj_t *s_btnm;
static lv_obj_t *s_back_btn;
static lv_obj_t *s_success_label;
static lv_timer_t *s_success_timer = NULL;

static bool s_pass_visible = false;
static bool s_qr_has_content = false;

static char s_input_buf[PASSWORD_MAX_LEN + 1];
static size_t s_input_len;

static keypad_purpose_t s_current_purpose;
static keypad_result_cb_t s_current_cb;

static void reset_input(void)
{
    s_input_len = 0;
    s_input_buf[0] = '\0';
    lv_textarea_set_text(s_textarea, "");
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_success_label,LV_OBJ_FLAG_HIDDEN);

    // رمز بعد از هر بار باز شدن دوباره، مخفی شروع شود
    s_pass_visible = false;
    lv_textarea_set_password_mode(s_textarea, true);
    lv_label_set_text(lv_obj_get_child(s_eye_btn, 0), LV_SYMBOL_EYE_OPEN);
}

static void show_success(const char *msg)
{
    lv_label_set_text(s_success_label, msg);
    lv_obj_clear_flag(s_success_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
}

static void success_timer_cb(lv_timer_t *timer)
{
    s_success_timer = NULL;
    keypad_screen_hide();
}

static void show_error(const char *msg)
{
    lv_label_set_text(s_error_label, msg);
    lv_obj_clear_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
}

static void eye_btn_event_cb(lv_event_t *e)
{
    s_pass_visible = !s_pass_visible;
    lv_textarea_set_password_mode(s_textarea, !s_pass_visible);
    lv_obj_t *icon = lv_obj_get_child(s_eye_btn, 0);
    lv_label_set_text(icon, s_pass_visible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
}

static void btnm_event_cb(lv_event_t *e)
{
    // گارد امنیتی: اگر overlay همین الان مخفی شده (مثلاً به‌خاطر یک رویداد
    // تاخیری از همون لمس قبلی)، این رویداد را نادیده بگیر
    if (lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    lv_obj_t *btnm = lv_event_get_target(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(btnm);
    const char *txt = lv_buttonmatrix_get_button_text(btnm, id);
    if (txt == NULL) {
        return;
    }

    if (strcmp(txt, "C") == 0) {
        reset_input();
        return;
    }

    if (strcmp(txt, "OK") == 0) {
        // از KEYPAD_PURPOSE_SETTINGS به بعد، رمز تنظیمات و رمز قفل درب دو
        // رمز کاملاً مستقل هستند (هرکدام جداگانه از منوی تنظیمات قابل
        // تغییرند) - پس باید نوع درست را به password_manager بدهیم.
        password_kind_t kind = (s_current_purpose == KEYPAD_PURPOSE_SETTINGS)
                                    ? PASSWORD_KIND_SETTINGS
                                    : PASSWORD_KIND_LOCK;
        bool ok = password_manager_verify(kind, s_input_buf);
        keypad_purpose_t purpose = s_current_purpose;
        keypad_result_cb_t cb = s_current_cb;

        if (ok) {
            ESP_LOGI(TAG, "Correct password entered");
            show_success("Access Granted");

            if (cb) {
                cb(purpose, true);
            }
            s_success_timer = lv_timer_create(success_timer_cb,1200,NULL);
            lv_timer_set_repeat_count(s_success_timer,1);
        } else {
            ESP_LOGW(TAG, "Wrong password entered");
            reset_input();
            show_error("Wrong code, try again");
            if (cb) {
                cb(purpose, false);
            }
        }
        return;
    }

    // رقم عددی
    if (s_input_len < PASSWORD_MAX_LEN) {
        s_input_buf[s_input_len++] = txt[0];
        s_input_buf[s_input_len] = '\0';
        lv_textarea_set_text(s_textarea, s_input_buf);
    }
}


static void back_btn_event_cb(lv_event_t *e)
{
    // انصراف بی‌سروصدا - بدون فراخوانی cb، پس هیچ رویداد "رمز غلط"
    // یا هر منطق دیگری ثبت نمی‌شود؛ فقط از این صفحه خارج می‌شویم
    keypad_screen_hide();
}

void keypad_screen_init(void)
{
    // overlay روی بالاترین لایه، مستقل از screen فعال - همیشه در دسترس است
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    // overlay باید جلوی لمس صفحه‌ی پشت را هم بگیرد
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    s_back_btn = lv_button_create(s_overlay);
    lv_obj_set_size(s_back_btn, 36, 36);
    lv_obj_align(s_back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(s_back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_back_btn, back_btn_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *back_label = lv_label_create(s_back_btn);
    lv_label_set_text(back_label, LV_SYMBOL_LEFT);
    lv_obj_center(back_label);


    s_title_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_title_label, lv_color_white(), 0);
    lv_label_set_text(s_title_label, "Enter Code");
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 12);

    // ===== تکست‌باکس رمز - بلافاصله بعد از عنوان =====
    s_textarea = lv_textarea_create(s_overlay);
    lv_obj_set_style_text_font(s_textarea, &lv_font_montserrat_28, 0);
    lv_textarea_set_password_mode(s_textarea, true);
    lv_textarea_set_one_line(s_textarea, true);
    lv_textarea_set_max_length(s_textarea, PASSWORD_MAX_LEN);
    lv_obj_set_width(s_textarea, 160);
    lv_obj_align(s_textarea, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_clear_flag(s_textarea, LV_OBJ_FLAG_CLICKABLE); // فقط نمایش؛ ورودی واقعی از button matrix می‌آید

    s_eye_btn = lv_button_create(s_overlay);
    lv_obj_set_size(s_eye_btn, 36, 36);
    lv_obj_align_to(s_eye_btn, s_textarea, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_add_event_cb(s_eye_btn, eye_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *eye_icon = lv_label_create(s_eye_btn);
    lv_label_set_text(eye_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(eye_icon);

    s_error_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_error_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_width(s_error_label, 220);        
    lv_obj_set_style_text_align(s_error_label,LV_TEXT_ALIGN_CENTER,0);                    
    lv_label_set_text(s_error_label, "");
    lv_obj_align_to(s_error_label, s_textarea,LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);


    s_success_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_success_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_width(s_success_label, 220);        
    lv_obj_set_style_text_align(s_success_label,LV_TEXT_ALIGN_CENTER,0); 
    lv_label_set_text(s_success_label, "");
    lv_obj_align_to(s_success_label, s_textarea,LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    lv_obj_add_flag(s_success_label, LV_OBJ_FLAG_HIDDEN);

    // ===== QR کد باز کردن قفل با چهره - بعد از textbox، قبل از کیبورد -
    // فقط برای KEYPAD_PURPOSE_UNLOCK نشان داده می‌شود =====
    s_qr_code = lv_qrcode_create(s_overlay);
    lv_qrcode_set_size(s_qr_code, 80);
    lv_qrcode_set_dark_color(s_qr_code, lv_color_black());
    lv_qrcode_set_light_color(s_qr_code, lv_color_white());
    lv_obj_align(s_qr_code, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_add_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);

    s_qr_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_qr_label, lv_color_white(), 0);
    lv_obj_set_style_text_align(s_qr_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_qr_label, "Scan to open with your face");
    lv_obj_align_to(s_qr_label, s_qr_code, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    lv_obj_add_flag(s_qr_label, LV_OBJ_FLAG_HIDDEN);


    s_btnm = lv_buttonmatrix_create(s_overlay);
    lv_buttonmatrix_set_map(s_btnm, s_btnm_map);
    lv_obj_set_size(s_btnm, 260, 220);
    lv_obj_align(s_btnm, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_btnm, btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // برای ورود رمز، رفتار "repeat" هنگام نگه‌داشتن دکمه (که برای کیبورد متنی
    // مثل Backspace طراحی شده) معنی نداره و باعث ثبت چندباره‌ی یک رقم/OK می‌شود
    // → برای تمام ۱۲ دکمه (۱-۹, C, ۰, OK) غیرفعالش می‌کنیم.
    lv_buttonmatrix_set_button_ctrl_all(s_btnm, LV_BUTTONMATRIX_CTRL_NO_REPEAT);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Keypad overlay initialized");
}

void keypad_screen_show(keypad_purpose_t purpose, keypad_result_cb_t on_result)
{
    s_current_purpose = purpose;
    s_current_cb = on_result;

    lv_label_set_text(s_title_label,
        purpose == KEYPAD_PURPOSE_UNLOCK ? "Enter Code to Unlock" : "Enter Code for Settings");

    // QR باز کردن با چهره فقط برای Unlock و فقط اگر وای‌فای وصله (محتوای معتبر دارد)
    if (purpose == KEYPAD_PURPOSE_UNLOCK && s_qr_has_content) {
        lv_obj_clear_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_qr_label, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_qr_label, LV_OBJ_FLAG_HIDDEN);
    }

    reset_input();
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void keypad_screen_hide(void)
{
    if (s_success_timer)
    {
        lv_timer_del(s_success_timer);
        s_success_timer = NULL;
    }
    
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    reset_input();
}

void keypad_screen_grant_access(void)
{
    // اگر کیبورد اصلاً باز نیست، کاری نکن
    if (s_overlay == NULL || lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }
    // فقط زمانی که کاربر در حال تلاش برای باز کردن قفل است (نه ورود به تنظیمات)
    if (s_current_purpose != KEYPAD_PURPOSE_UNLOCK) {
        return;
    }

    // نمایش پیام موفقیت (دقیقاً همون لیبلی که موقع رمز درست نمایش داده می‌شد)
    show_success("Access Granted");

    // اگر تایمر از قبل وجود داره (مثلاً کاربر همزمان رمز رو هم درست زده بود)، پاکش کن
    if (s_success_timer) {
        lv_timer_del(s_success_timer);
        s_success_timer = NULL;
    }
    
    // راه‌اندازی تایمر برای بستن خودکار صفحه بعد از ۱.۲ ثانیه
    s_success_timer = lv_timer_create(success_timer_cb, 1200, NULL);
    lv_timer_set_repeat_count(s_success_timer, 1);
}

void keypad_screen_update_capture_qr(const char *url)
{
    if (s_qr_code == NULL) {
        return;
    }

    if (url == NULL || url[0] == '\0') {
        s_qr_has_content = false;
        lv_obj_add_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_qr_label, LV_OBJ_FLAG_HIDDEN);
        return;
    }

    lv_qrcode_update(s_qr_code, url, strlen(url));
    s_qr_has_content = true;

    // اگر overlay همین الان با هدف Unlock باز است، فوراً نشانش بده
    if (!lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN) && s_current_purpose == KEYPAD_PURPOSE_UNLOCK) {
        lv_obj_clear_flag(s_qr_code, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_qr_label, LV_OBJ_FLAG_HIDDEN);
    }
}