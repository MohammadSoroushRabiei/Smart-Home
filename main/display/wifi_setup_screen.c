#include "wifi_setup_screen.h"
#include "wifi_manager.h"
#include "lcd_driver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "lvgl.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "wifi_setup_screen";

#define WIFI_SETUP_SCAN_MAX   20

static lv_obj_t *s_overlay;

// --- زیرصفحه‌ی لیست SSID ---
static lv_obj_t *s_list_view;
static lv_obj_t *s_list_back_btn;
static lv_obj_t *s_list_title;
static lv_obj_t *s_list_status_label;   // "در حال اسکن..." / "شبکه‌ای پیدا نشد"
static lv_obj_t *s_list;
static lv_obj_t *s_rescan_btn;

// --- زیرصفحه‌ی ورود رمز ---
static lv_obj_t *s_pass_view;
static lv_obj_t *s_pass_back_btn;
static lv_obj_t *s_pass_title;
static lv_obj_t *s_pass_textarea;
static lv_obj_t *s_pass_status_label;
static lv_obj_t *s_pass_keyboard;

static wifi_scan_result_t s_scan_results[WIFI_SETUP_SCAN_MAX];
static int s_scan_count = 0;
static char s_selected_ssid[33];

static void show_list_view(void);
static void show_password_view(const char *ssid);
static void start_scan(void);
static void ssid_btn_event_cb(lv_event_t *e);

// ---------------------------------------------------------------------
// تسک پس‌زمینه‌ی اسکن - چون wifi_manager_scan بلاک‌کننده است، هرگز از
// تسک LVGL صدا زده نمی‌شود.
// ---------------------------------------------------------------------
static void scan_task(void *arg)
{
    int n = wifi_manager_scan(s_scan_results, WIFI_SETUP_SCAN_MAX);

    lcd_driver_lvgl_lock();
    s_scan_count = n;

    lv_obj_clean(s_list);
    if (n == 0) {
        lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_list_status_label, "No Network");
    } else {
        lv_obj_add_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
        for (int i = 0; i < n; i++) {
            char buf[48];
            const char *lock_mark = (s_scan_results[i].authmode == WIFI_AUTH_OPEN) ? "" : LV_SYMBOL_SETTINGS " ";
            snprintf(buf, sizeof(buf), "%s%s (%d dBm)", lock_mark, s_scan_results[i].ssid, s_scan_results[i].rssi);

            lv_obj_t *btn = lv_list_add_button(s_list, LV_SYMBOL_WIFI, buf);
            lv_obj_add_event_cb(btn, ssid_btn_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        }
    }

    lcd_driver_lvgl_unlock();
    vTaskDelete(NULL);
}

static void ssid_btn_event_cb(lv_event_t *e)
{
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_scan_count) {
        return;
    }

    if (s_scan_results[idx].authmode == WIFI_AUTH_OPEN) {
        // شبکه‌ی باز - نیازی به رمز نیست، مستقیم تلاش برای اتصال
        strncpy(s_selected_ssid, s_scan_results[idx].ssid, sizeof(s_selected_ssid) - 1);
        show_password_view(s_selected_ssid);
        lv_textarea_set_text(s_pass_textarea, "");
        // مستقیم submit کن چون رمزی لازم نیست
        lv_obj_send_event(s_pass_keyboard, LV_EVENT_READY, NULL);
    } else {
        strncpy(s_selected_ssid, s_scan_results[idx].ssid, sizeof(s_selected_ssid) - 1);
        show_password_view(s_selected_ssid);
    }
}

static void start_scan(void)
{
    lv_obj_clean(s_list);
    lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(s_list_status_label, "Scanning ...");

    xTaskCreate(scan_task, "wifi_scan_task", 4096, NULL, 3, NULL);
}

static void rescan_btn_event_cb(lv_event_t *e)
{
    start_scan();
}

static void list_back_btn_event_cb(lv_event_t *e)
{
    wifi_setup_screen_hide();
    wifi_manager_enable();   // اگر آفلاین بودیم، برگرد به تلاش خودکار روی لیست شناخته‌شده
}

// ---------------------------------------------------------------------
// تسک پس‌زمینه‌ی اتصال - چون wifi_manager_connect_and_save بلاک‌کننده
// است (تا ۱۵ ثانیه)، هرگز از تسک LVGL صدا زده نمی‌شود.
// ---------------------------------------------------------------------
typedef struct {
    char ssid[33];
    char password[65];
} connect_task_arg_t;

static void hide_screen_timer_cb(lv_timer_t *t)
{
    wifi_setup_screen_hide();
}

static void connect_task(void *arg)
{
    connect_task_arg_t *a = (connect_task_arg_t *)arg;
    esp_err_t ret = wifi_manager_connect_and_save(a->ssid, a->password);

    lcd_driver_lvgl_lock();
    if (ret == ESP_OK) {
        lv_obj_set_style_text_color(s_pass_status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_label_set_text(s_pass_status_label, "Connected!");
        // یه تایمر کوتاه برای بستن خودکار صفحه بعد از موفقیت
        lv_timer_t *t = lv_timer_create(hide_screen_timer_cb, 1000, NULL);
        lv_timer_set_repeat_count(t, 1);
    } else {
        lv_obj_set_style_text_color(s_pass_status_label, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(s_pass_status_label, "اتصال ناموفق - رمز یا شبکه را بررسی کنید");
    }
    lv_obj_clear_flag(s_pass_keyboard, LV_OBJ_FLAG_HIDDEN);
    lcd_driver_lvgl_unlock();

    free(a);
    vTaskDelete(NULL);
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_READY) {
        return;
    }

    const char *password = lv_textarea_get_text(s_pass_textarea);

    connect_task_arg_t *arg = malloc(sizeof(connect_task_arg_t));
    if (arg == NULL) {
        return;
    }
    strncpy(arg->ssid, s_selected_ssid, sizeof(arg->ssid) - 1);
    arg->ssid[sizeof(arg->ssid) - 1] = '\0';
    strncpy(arg->password, password, sizeof(arg->password) - 1);
    arg->password[sizeof(arg->password) - 1] = '\0';

    lv_obj_set_style_text_color(s_pass_status_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_pass_status_label, "Connecting ...");
    lv_obj_add_flag(s_pass_keyboard, LV_OBJ_FLAG_HIDDEN);   // تا پایان تلاش، اجازه‌ی ورود دوباره نده

    xTaskCreate(connect_task, "wifi_connect_task", 4096, arg, 3, NULL);
}

static void pass_back_btn_event_cb(lv_event_t *e)
{
    show_list_view();
}

static void show_list_view(void)
{
    lv_obj_add_flag(s_pass_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_list_view, LV_OBJ_FLAG_HIDDEN);
}

static void show_password_view(const char *ssid)
{
    lv_obj_add_flag(s_list_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_pass_view, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(s_pass_keyboard, LV_OBJ_FLAG_HIDDEN);

    char title[48];
    snprintf(title, sizeof(title), "password of %s", ssid);
    lv_label_set_text(s_pass_title, title);

    lv_textarea_set_text(s_pass_textarea, "");
    lv_label_set_text(s_pass_status_label, "");
}

void wifi_setup_screen_init(void)
{
    s_overlay = lv_obj_create(lv_layer_top());
    lv_obj_remove_style_all(s_overlay);
    lv_obj_set_size(s_overlay, LV_PCT(100), LV_PCT(100));
    lv_obj_set_style_bg_color(s_overlay, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(s_overlay, LV_OPA_90, 0);
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_CLICKABLE);

    // ===== زیرصفحه‌ی لیست SSID =====
    s_list_view = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_list_view);
    lv_obj_set_size(s_list_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(s_list_view, LV_OBJ_FLAG_SCROLLABLE);

    s_list_back_btn = lv_button_create(s_list_view);
    lv_obj_set_size(s_list_back_btn, 36, 36);
    lv_obj_align(s_list_back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(s_list_back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_list_back_btn, list_back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label1 = lv_label_create(s_list_back_btn);
    lv_label_set_text(back_label1, LV_SYMBOL_LEFT);
    lv_obj_center(back_label1);

    s_rescan_btn = lv_button_create(s_list_view);
    lv_obj_set_size(s_rescan_btn, 36, 36);
    lv_obj_align(s_rescan_btn, LV_ALIGN_TOP_RIGHT, -5, 5);
    lv_obj_set_style_bg_color(s_rescan_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_rescan_btn, rescan_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *rescan_label = lv_label_create(s_rescan_btn);
    lv_label_set_text(rescan_label, LV_SYMBOL_REFRESH);
    lv_obj_center(rescan_label);

    s_list_title = lv_label_create(s_list_view);
    lv_obj_set_style_text_color(s_list_title, lv_color_white(), 0);
    lv_label_set_text(s_list_title, "select wifi network");
    lv_obj_align(s_list_title, LV_ALIGN_TOP_MID, 0, 15);

    s_list_status_label = lv_label_create(s_list_view);
    lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
    lv_label_set_text(s_list_status_label, "");
    lv_obj_align(s_list_status_label, LV_ALIGN_TOP_MID, 0, 55);
    lv_obj_add_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);

    s_list = lv_list_create(s_list_view);
    lv_obj_set_size(s_list, LV_PCT(90), 380);
    lv_obj_align(s_list, LV_ALIGN_TOP_MID, 0, 85);

    // ===== زیرصفحه‌ی ورود رمز =====
    s_pass_view = lv_obj_create(s_overlay);
    lv_obj_remove_style_all(s_pass_view);
    lv_obj_set_size(s_pass_view, LV_PCT(100), LV_PCT(100));
    lv_obj_clear_flag(s_pass_view, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_pass_view, LV_OBJ_FLAG_HIDDEN);

    s_pass_back_btn = lv_button_create(s_pass_view);
    lv_obj_set_size(s_pass_back_btn, 36, 36);
    lv_obj_align(s_pass_back_btn, LV_ALIGN_TOP_LEFT, 5, 5);
    lv_obj_set_style_bg_color(s_pass_back_btn, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(s_pass_back_btn, pass_back_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_label2 = lv_label_create(s_pass_back_btn);
    lv_label_set_text(back_label2, LV_SYMBOL_LEFT);
    lv_obj_center(back_label2);

    s_pass_title = lv_label_create(s_pass_view);
    lv_obj_set_style_text_color(s_pass_title, lv_color_white(), 0);
    lv_label_set_text(s_pass_title, "network password");
    lv_obj_align(s_pass_title, LV_ALIGN_TOP_MID, 0, 15);

    s_pass_textarea = lv_textarea_create(s_pass_view);
    lv_textarea_set_password_mode(s_pass_textarea, true);
    lv_textarea_set_one_line(s_pass_textarea, true);
    lv_textarea_set_max_length(s_pass_textarea, 64);
    lv_obj_set_width(s_pass_textarea, LV_PCT(85));
    lv_obj_align(s_pass_textarea, LV_ALIGN_TOP_MID, 0, 55);

    s_pass_status_label = lv_label_create(s_pass_view);
    lv_obj_set_width(s_pass_status_label, LV_PCT(90));
    lv_obj_set_style_text_align(s_pass_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_pass_status_label, "");
    lv_obj_align_to(s_pass_status_label, s_pass_textarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 8);

    s_pass_keyboard = lv_keyboard_create(s_pass_view);
    lv_keyboard_set_textarea(s_pass_keyboard, s_pass_textarea);
    lv_obj_set_size(s_pass_keyboard, LV_PCT(100), 240);
    lv_obj_align(s_pass_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s_pass_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);

    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);

    ESP_LOGI(TAG, "WiFi setup screen initialized");
}

void wifi_setup_screen_show(void)
{
    show_list_view();
    lv_obj_clear_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_overlay);
    start_scan();
}

void wifi_setup_screen_hide(void)
{
    lv_obj_add_flag(s_overlay, LV_OBJ_FLAG_HIDDEN);
}