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
static lv_obj_t *s_textarea;
static lv_obj_t *s_error_label;
static lv_obj_t *s_btnm;

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
}

static void show_error(const char *msg)
{
    lv_label_set_text(s_error_label, msg);
    lv_obj_clear_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);
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
        bool ok = password_manager_verify(s_input_buf);
        keypad_purpose_t purpose = s_current_purpose;
        keypad_result_cb_t cb = s_current_cb;

        if (ok) {
            keypad_screen_hide();
            if (cb) {
                cb(purpose, true);
            }
        } else {
            ESP_LOGW(TAG, "Wrong password entered");
            show_error("Wrong code, try again");
            reset_input();
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

    s_title_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_title_label, lv_color_white(), 0);
    lv_label_set_text(s_title_label, "Enter Code");
    lv_obj_align(s_title_label, LV_ALIGN_TOP_MID, 0, 20);

    s_textarea = lv_textarea_create(s_overlay);
    lv_textarea_set_password_mode(s_textarea, true);
    lv_textarea_set_one_line(s_textarea, true);
    lv_textarea_set_max_length(s_textarea, PASSWORD_MAX_LEN);
    lv_obj_set_width(s_textarea, 200);
    lv_obj_align(s_textarea, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_clear_flag(s_textarea, LV_OBJ_FLAG_CLICKABLE); // فقط نمایش؛ ورودی واقعی از button matrix می‌آید

    s_error_label = lv_label_create(s_overlay);
    lv_obj_set_style_text_color(s_error_label, lv_palette_main(LV_PALETTE_RED), 0);
    lv_label_set_text(s_error_label, "");
    lv_obj_align(s_error_label, LV_ALIGN_TOP_MID, 0, 90);
    lv_obj_add_flag(s_error_label, LV_OBJ_FLAG_HIDDEN);

    s_btnm = lv_buttonmatrix_create(s_overlay);
    lv_buttonmatrix_set_map(s_btnm, s_btnm_map);
    lv_obj_set_size(s_btnm, 260, 220);
    lv_obj_align(s_btnm, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(s_btnm, btnm_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // برای ورود رمز، رفتار "repeat" هنگام نگه‌داشتن دکمه (که برای کیبورد متنی
    // مثل Backspace طراحی شده) معنی نداره و باعث ثبت چندباره‌ی یک رقم/OK می‌شود
    // → برای تمام ۱۲ دکمه (۱-۹, C, ۰, OK) غیرفعالش می‌کنیم.
    for (uint32_t i = 0; i < 12; i++) {
        lv_buttonmatrix_set_button_ctrl(s_btnm, i, LV_BUTTONMATRIX_CTRL_NO_REPEAT);
    }

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "Keypad overlay initialized");
}

void keypad_screen_show(keypad_purpose_t purpose, keypad_result_cb_t on_result)
{
    s_current_purpose = purpose;
    s_current_cb = on_result;

    lv_label_set_text(s_title_label,
        purpose == KEYPAD_PURPOSE_UNLOCK ? "Enter Code to Unlock" : "Enter Code for Settings");

    reset_input();
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}

void keypad_screen_hide(void)
{
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    reset_input();
}