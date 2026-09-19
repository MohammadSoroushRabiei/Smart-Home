#include "setting_screen.h"
#include "enroll_token.h"
#include "wifi_manager.h"
#include "password_manager.h"
#include <stdio.h>
#include <string.h>
#include "lvgl.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "face_db.h"
#include "face_recognition.h"

static const char *TAG = "settings_screen";

static lv_obj_t *s_overlay;
static lv_obj_t *s_menu_view;
static lv_obj_t *s_enroll_view;
static lv_obj_t *s_qr_code;
static lv_obj_t *s_countdown_label;
static lv_timer_t *s_countdown_timer = NULL;
static int64_t s_token_expiry_us = 0;
static lv_obj_t *s_faces_view;
static lv_obj_t *s_faces_list;
// لیست تجمیع‌شده‌ی افراد؛ نامِ در حال حذف بین کلیک و تایید مودال نگه داشته می‌شود
static face_db_person_t s_persons[FACE_DB_MAX_ENTRIES];
static char s_pending_delete_name[FACE_DB_NAME_MAX_LEN + 1];
static size_t s_pending_delete_count = 0;

// ===== نمای تغییر رمز (مشترک بین رمز قفل و رمز تنظیمات) =====
// نقشه‌ی دکمه‌های کیبورد - همانند keypad_screen.c
static const char *s_pw_btnm_map[] = {
    "1", "2", "3", "\n",
    "4", "5", "6", "\n",
    "7", "8", "9", "\n",
    "C", "0", "OK", ""
};

typedef enum {
    PW_STAGE_CURRENT,   // تایید رمز فعلی
    PW_STAGE_NEW,       // ورود رمز جدید
    PW_STAGE_CONFIRM,   // تکرار رمز جدید برای تایید
} pw_stage_t;

static lv_obj_t *s_pw_view;
static lv_obj_t *s_pw_title;
static lv_obj_t *s_pw_textarea;
static lv_obj_t *s_pw_eye_btn;
static lv_obj_t *s_pw_error_label;
static lv_obj_t *s_pw_success_label;
static lv_obj_t *s_pw_btnm;
static lv_timer_t *s_pw_success_timer = NULL;

static bool s_pw_visible = false;
static char s_pw_input_buf[PASSWORD_MAX_LEN + 1];
static size_t s_pw_input_len = 0;

static pw_stage_t s_pw_stage;
static password_kind_t s_pw_kind;
// مقدار موقت رمز جدید - فقط بین مرحله‌ی NEW و CONFIRM نگه داشته می‌شود و
// به‌محض خروج از این نما (موفق، ناموفق یا انصراف) پاک می‌شود.
static char s_pw_new_value[PASSWORD_MAX_LEN + 1];


static void populate_faces_list(void);
static void delete_face_btn_event_cb(lv_event_t *e);
static void start_change_password(password_kind_t kind);
static void reset_pw_input(void);


// ---------------------------------------------------------------------
// جابه‌جایی بین دو نما
// ---------------------------------------------------------------------

static void show_menu_view(void)
{
    if (s_countdown_timer) {
        lv_timer_del(s_countdown_timer);
        s_countdown_timer = NULL;
    }
    if (s_pw_success_timer) {
        lv_timer_del(s_pw_success_timer);
        s_pw_success_timer = NULL;
    }
    lv_obj_add_flag(s_enroll_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_faces_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_pw_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_menu_view, LV_OBJ_FLAG_HIDDEN);

    // هر مقدار موقت رمز جدیدی که ممکن است در حافظه مانده باشد را پاک کن
    memset(s_pw_new_value, 0, sizeof(s_pw_new_value));
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

static void confirm_cancel_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);
    lv_msgbox_close(mbox);
}

static void confirm_delete_btn_event_cb(lv_event_t *e)
{
    lv_obj_t *mbox = (lv_obj_t *)lv_event_get_user_data(e);

    // همه‌ی نمونه‌های این شخص (هر کدام یک feature مستقل) حذف می‌شوند
    uint16_t removed_ids[FACE_DB_MAX_ENTRIES];
    size_t removed = 0;
    if (face_db_remove_by_name(s_pending_delete_name, removed_ids,
                               FACE_DB_MAX_ENTRIES, &removed) == ESP_OK) {
        for (size_t i = 0; i < removed; i++) {
            face_recognition_delete(removed_ids[i]);
        }
    }
    lv_msgbox_close(mbox);
    populate_faces_list();
}

static void populate_faces_list(void)
{
    lv_obj_clean(s_faces_list);

    size_t n = face_db_get_persons(s_persons, FACE_DB_MAX_ENTRIES);

    if (n == 0) {
        lv_list_add_text(s_faces_list, "No faces enrolled yet");
        return;
    }

    for (size_t i = 0; i < n; i++) {
        char buf[56];
        snprintf(buf, sizeof(buf), "%s  (%u sample%s)",
                 s_persons[i].name, (unsigned)s_persons[i].sample_count,
                 s_persons[i].sample_count == 1 ? "" : "s");

        lv_obj_t *btn = lv_list_add_button(s_faces_list, LV_SYMBOL_TRASH, NULL);

        /* آیکون سطل زباله قرمز */
        lv_obj_set_style_text_color(btn, lv_color_hex(0xFF0000), LV_PART_MAIN);

        lv_obj_t *label = lv_label_create(btn);
        lv_obj_set_style_text_color(label, lv_color_hex(0x000000), LV_PART_MAIN);
        lv_label_set_text(label, buf);

        lv_obj_add_event_cb(btn, delete_face_btn_event_cb, LV_EVENT_CLICKED,
                             (void *)(uintptr_t)i);
    }
}

static void delete_face_btn_event_cb(lv_event_t *e)
{
    size_t idx = (size_t)(uintptr_t)lv_event_get_user_data(e);
    strncpy(s_pending_delete_name, s_persons[idx].name, sizeof(s_pending_delete_name) - 1);
    s_pending_delete_name[sizeof(s_pending_delete_name) - 1] = '\0';
    s_pending_delete_count = s_persons[idx].sample_count;

    lv_obj_t *mbox = lv_msgbox_create(NULL);   // NULL = روی بالاترین لایه، مودال
    lv_msgbox_add_title(mbox, "Confirm Delete");

    char text[80];
    snprintf(text, sizeof(text), "Delete all %u sample(s) of \"%s\"?",
             (unsigned)s_pending_delete_count, s_pending_delete_name);
    lv_msgbox_add_text(mbox, text);

    lv_obj_t *cancel_btn = lv_msgbox_add_footer_button(mbox, "Cancel");
    lv_obj_t *delete_btn = lv_msgbox_add_footer_button(mbox, "Delete");
    lv_obj_set_style_bg_color(delete_btn, lv_palette_main(LV_PALETTE_RED), 0);

    lv_obj_add_event_cb(cancel_btn, confirm_cancel_btn_event_cb, LV_EVENT_CLICKED, mbox);
    lv_obj_add_event_cb(delete_btn, confirm_delete_btn_event_cb, LV_EVENT_CLICKED, mbox);
}

static void show_faces_view(void)
{
    populate_faces_list();
    lv_obj_add_flag(s_menu_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_faces_view, LV_OBJ_FLAG_HIDDEN);
}

static void manage_faces_btn_event_cb(lv_event_t *e)
{
    show_faces_view();
}

static void faces_back_btn_event_cb(lv_event_t *e)
{
    show_menu_view();
}

// ---------------------------------------------------------------------
// نمای تغییر رمز - قابل استفاده هم برای رمز قفل درب هم رمز منوی تنظیمات
// ---------------------------------------------------------------------

static void reset_pw_input(void)
{
    s_pw_input_len = 0;
    s_pw_input_buf[0] = '\0';
    lv_textarea_set_text(s_pw_textarea, "");
    lv_obj_add_flag(s_pw_error_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_pw_success_label, LV_OBJ_FLAG_HIDDEN);

    s_pw_visible = false;
    lv_textarea_set_password_mode(s_pw_textarea, true);
    lv_label_set_text(lv_obj_get_child(s_pw_eye_btn, 0), LV_SYMBOL_EYE_OPEN);
}

static void show_pw_error(const char *msg)
{
    lv_label_set_text(s_pw_error_label, msg);
    lv_obj_clear_flag(s_pw_error_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_pw_success_label, LV_OBJ_FLAG_HIDDEN);
}

static void show_pw_success(const char *msg)
{
    lv_label_set_text(s_pw_success_label, msg);
    lv_obj_clear_flag(s_pw_success_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_pw_error_label, LV_OBJ_FLAG_HIDDEN);
}

static void pw_success_timer_cb(lv_timer_t *timer)
{
    s_pw_success_timer = NULL;
    // بعد از تغییر موفق رمز، به منوی تنظیمات برمی‌گردیم (نه صفحه‌ی اصلی)
    show_menu_view();
}

static void start_change_password(password_kind_t kind)
{
    s_pw_kind = kind;
    s_pw_stage = PW_STAGE_CURRENT;
    memset(s_pw_new_value, 0, sizeof(s_pw_new_value));
    reset_pw_input();

    lv_label_set_text(s_pw_title,
        kind == PASSWORD_KIND_SETTINGS ? "Current Settings Password" : "Current Lock Password");

    lv_obj_add_flag(s_menu_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_pw_view, LV_OBJ_FLAG_HIDDEN);
}

static void change_settings_pw_btn_event_cb(lv_event_t *e)
{
    start_change_password(PASSWORD_KIND_SETTINGS);
}

static void change_lock_pw_btn_event_cb(lv_event_t *e)
{
    start_change_password(PASSWORD_KIND_LOCK);
}

static void pw_back_btn_event_cb(lv_event_t *e)
{
    // انصراف در هر مرحله - چیزی ذخیره نمی‌شود، فقط به منوی تنظیمات برمی‌گردیم
    show_menu_view();
}

static void pw_eye_btn_event_cb(lv_event_t *e)
{
    s_pw_visible = !s_pw_visible;
    lv_textarea_set_password_mode(s_pw_textarea, !s_pw_visible);
    lv_obj_t *icon = lv_obj_get_child(s_pw_eye_btn, 0);
    lv_label_set_text(icon, s_pw_visible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
}

static void pw_btnm_event_cb(lv_event_t *e)
{
    if (lv_obj_has_flag(s_pw_view, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    lv_obj_t *btnm = lv_event_get_target(e);
    uint32_t id = lv_buttonmatrix_get_selected_button(btnm);
    const char *txt = lv_buttonmatrix_get_button_text(btnm, id);
    if (txt == NULL) {
        return;
    }

    if (strcmp(txt, "C") == 0) {
        reset_pw_input();
        return;
    }

    if (strcmp(txt, "OK") == 0) {
        switch (s_pw_stage) {

        case PW_STAGE_CURRENT:
            if (password_manager_verify(s_pw_kind, s_pw_input_buf)) {
                s_pw_stage = PW_STAGE_NEW;
                reset_pw_input();
                lv_label_set_text(s_pw_title, "Enter New Password");
            } else {
                ESP_LOGW(TAG, "Password change rejected: current password incorrect");
                reset_pw_input();
                show_pw_error("Wrong password, try again");
            }
            break;

        case PW_STAGE_NEW:
            if (s_pw_input_len < PASSWORD_MIN_LEN) {
                show_pw_error("Too short (min 4 digits)");
                reset_pw_input();
            } else {
                strncpy(s_pw_new_value, s_pw_input_buf, sizeof(s_pw_new_value) - 1);
                s_pw_new_value[sizeof(s_pw_new_value) - 1] = '\0';
                s_pw_stage = PW_STAGE_CONFIRM;
                reset_pw_input();
                lv_label_set_text(s_pw_title, "Confirm New Password");
            }
            break;

        case PW_STAGE_CONFIRM:
            if (strcmp(s_pw_input_buf, s_pw_new_value) != 0) {
                show_pw_error("Passwords do not match");
                s_pw_stage = PW_STAGE_NEW;
                memset(s_pw_new_value, 0, sizeof(s_pw_new_value));
                reset_pw_input();
                lv_label_set_text(s_pw_title, "Enter New Password");
            } else {
                esp_err_t ret = password_manager_set(s_pw_kind, s_pw_new_value);
                memset(s_pw_new_value, 0, sizeof(s_pw_new_value));

                if (ret == ESP_OK) {
                    reset_pw_input();
                    show_pw_success("Password Changed");
                    if (s_pw_success_timer) {
                        lv_timer_del(s_pw_success_timer);
                    }
                    s_pw_success_timer = lv_timer_create(pw_success_timer_cb, 1200, NULL);
                    lv_timer_set_repeat_count(s_pw_success_timer, 1);
                } else {
                    s_pw_stage = PW_STAGE_NEW;
                    reset_pw_input();
                    show_pw_error("Failed to save, try again");
                    lv_label_set_text(s_pw_title, "Enter New Password");
                }
            }
            break;
        }
        return;
    }

    // رقم عددی
    if (s_pw_input_len < PASSWORD_MAX_LEN) {
        s_pw_input_buf[s_pw_input_len++] = txt[0];
        s_pw_input_buf[s_pw_input_len] = '\0';
        lv_textarea_set_text(s_pw_textarea, s_pw_input_buf);
    }
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

    lv_obj_t *enroll_btn = lv_button_create(s_menu_view);
    lv_obj_set_size(enroll_btn, 220, 55);
    lv_obj_align(enroll_btn, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_set_style_bg_color(enroll_btn, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_add_event_cb(enroll_btn, enroll_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *enroll_label = lv_label_create(enroll_btn);
    lv_label_set_text(enroll_label, "Enroll New Face");
    lv_obj_center(enroll_label);
    
    lv_obj_t *manage_btn = lv_button_create(s_menu_view);
    lv_obj_set_size(manage_btn, 220, 55);
    lv_obj_align(manage_btn, LV_ALIGN_TOP_MID, 0, 155);
    lv_obj_set_style_bg_color(manage_btn, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_event_cb(manage_btn, manage_faces_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *manage_label = lv_label_create(manage_btn);
    lv_label_set_text(manage_label, "Manage Faces");
    lv_obj_center(manage_label);

    lv_obj_t *change_settings_pw_btn = lv_button_create(s_menu_view);
    lv_obj_set_size(change_settings_pw_btn, 220, 55);
    lv_obj_align(change_settings_pw_btn, LV_ALIGN_TOP_MID, 0, 220);
    lv_obj_set_style_bg_color(change_settings_pw_btn, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_add_event_cb(change_settings_pw_btn, change_settings_pw_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *change_settings_pw_label = lv_label_create(change_settings_pw_btn);
    lv_label_set_text(change_settings_pw_label, "Change Settings Password");
    lv_obj_set_style_text_align(change_settings_pw_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(change_settings_pw_label, 200);
    lv_obj_center(change_settings_pw_label);

    lv_obj_t *change_lock_pw_btn = lv_button_create(s_menu_view);
    lv_obj_set_size(change_lock_pw_btn, 220, 55);
    lv_obj_align(change_lock_pw_btn, LV_ALIGN_TOP_MID, 0, 285);
    lv_obj_set_style_bg_color(change_lock_pw_btn, lv_palette_main(LV_PALETTE_DEEP_PURPLE), 0);
    lv_obj_add_event_cb(change_lock_pw_btn, change_lock_pw_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *change_lock_pw_label = lv_label_create(change_lock_pw_btn);
    lv_label_set_text(change_lock_pw_label, "Change Door Lock Password");
    lv_obj_set_style_text_align(change_lock_pw_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(change_lock_pw_label, 200);
    lv_obj_center(change_lock_pw_label);

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

    // ===== نمای Manage Faces =====
    s_faces_view = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_faces_view);
    lv_obj_set_size(s_faces_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(s_faces_view, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *faces_back_btn = lv_button_create(s_faces_view);
    lv_obj_set_size(faces_back_btn, 36, 36);
    lv_obj_align(faces_back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(faces_back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(faces_back_btn, faces_back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *faces_back_label = lv_label_create(faces_back_btn);
    lv_label_set_text(faces_back_label, LV_SYMBOL_LEFT);
    lv_obj_center(faces_back_label);

    lv_obj_t *faces_title = lv_label_create(s_faces_view);
    lv_obj_set_style_text_color(faces_title, lv_color_white(), 0);
    lv_label_set_text(faces_title, "Manage Faces");
    lv_obj_align(faces_title, LV_ALIGN_TOP_MID, 0, 20);

    s_faces_list = lv_list_create(s_faces_view);
    lv_obj_set_size(s_faces_list, 280, 350);
    lv_obj_align(s_faces_list, LV_ALIGN_BOTTOM_MID, 0, -15);

    lv_obj_add_flag(s_faces_view, LV_OBJ_FLAG_HIDDEN);

    // ===== نمای تغییر رمز (مشترک بین قفل و تنظیمات) =====
    s_pw_view = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_pw_view);
    lv_obj_set_size(s_pw_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(s_pw_view, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *pw_back_btn = lv_button_create(s_pw_view);
    lv_obj_set_size(pw_back_btn, 36, 36);
    lv_obj_align(pw_back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(pw_back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(pw_back_btn, pw_back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pw_back_label = lv_label_create(pw_back_btn);
    lv_label_set_text(pw_back_label, LV_SYMBOL_LEFT);
    lv_obj_center(pw_back_label);

    s_pw_title = lv_label_create(s_pw_view);
    lv_obj_set_style_text_color(s_pw_title, lv_color_white(), 0);
    lv_label_set_text(s_pw_title, "Current Password");
    lv_obj_align(s_pw_title, LV_ALIGN_TOP_MID, 0, 12);

    s_pw_textarea = lv_textarea_create(s_pw_view);
    lv_obj_set_style_text_font(s_pw_textarea, &lv_font_montserrat_28, 0);
    lv_textarea_set_password_mode(s_pw_textarea, true);
    lv_textarea_set_one_line(s_pw_textarea, true);
    lv_textarea_set_max_length(s_pw_textarea, PASSWORD_MAX_LEN);
    lv_obj_set_width(s_pw_textarea, 160);
    lv_obj_align(s_pw_textarea, LV_ALIGN_TOP_MID, 0, 45);
    lv_obj_clear_flag(s_pw_textarea, LV_OBJ_FLAG_CLICKABLE);

    s_pw_eye_btn = lv_button_create(s_pw_view);
    lv_obj_set_size(s_pw_eye_btn, 36, 36);
    lv_obj_align_to(s_pw_eye_btn, s_pw_textarea, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_add_event_cb(s_pw_eye_btn, pw_eye_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *pw_eye_icon = lv_label_create(s_pw_eye_btn);
    lv_label_set_text(pw_eye_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(pw_eye_icon);

    s_pw_error_label = lv_label_create(s_pw_view);
    lv_obj_set_style_text_color(s_pw_error_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_width(s_pw_error_label, 220);
    lv_obj_set_style_text_align(s_pw_error_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_pw_error_label, "");
    lv_obj_align_to(s_pw_error_label, s_pw_textarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(s_pw_error_label, LV_OBJ_FLAG_HIDDEN);

    s_pw_success_label = lv_label_create(s_pw_view);
    lv_obj_set_style_text_color(s_pw_success_label, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_set_width(s_pw_success_label, 220);
    lv_obj_set_style_text_align(s_pw_success_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_pw_success_label, "");
    lv_obj_align_to(s_pw_success_label, s_pw_textarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 3);
    lv_obj_add_flag(s_pw_success_label, LV_OBJ_FLAG_HIDDEN);

    s_pw_btnm = lv_buttonmatrix_create(s_pw_view);
    lv_buttonmatrix_set_map(s_pw_btnm, s_pw_btnm_map);
    lv_obj_set_size(s_pw_btnm, 260, 220);
    lv_obj_align(s_pw_btnm, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_pw_btnm, pw_btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_buttonmatrix_set_button_ctrl_all(s_pw_btnm, LV_BUTTONMATRIX_CTRL_NO_REPEAT);

    lv_obj_add_flag(s_pw_view, LV_OBJ_FLAG_HIDDEN);

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
    if (s_pw_success_timer) {
        lv_timer_del(s_pw_success_timer);
        s_pw_success_timer = NULL;
    }
    enroll_token_invalidate();
    memset(s_pw_new_value, 0, sizeof(s_pw_new_value));

    // بازگشت به نمای منو برای دفعه‌ی بعد که Settings باز می‌شود
    lv_obj_add_flag(s_enroll_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_faces_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_pw_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_menu_view, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}