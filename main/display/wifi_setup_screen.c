#include "wifi_setup_screen.h"
#include "wifi_manager.h"
#include "wifi_config.h"
#include "lcd_driver.h"
#include "mqtt_manager.h"
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
static lv_obj_t *s_list_status_label;   // "Scanning..." / "No networks found" / نتیجه‌ی اتصال
static lv_obj_t *s_list;
static lv_obj_t *s_rescan_btn;

// --- زیرصفحه‌ی ورود رمز ---
static lv_obj_t *s_pass_view;
static lv_obj_t *s_pass_back_btn;
static lv_obj_t *s_pass_title;
static lv_obj_t *s_pass_textarea;
static lv_obj_t *s_pass_eye_btn;
static bool s_pass_visible = false;
static lv_obj_t *s_pass_status_label;
static lv_obj_t *s_pass_keyboard;

static wifi_scan_result_t s_scan_results[WIFI_SETUP_SCAN_MAX];
static bool s_scan_is_known[WIFI_SETUP_SCAN_MAX];
static char s_scan_known_password[WIFI_SETUP_SCAN_MAX][65];
static int s_scan_count = 0;
static char s_selected_ssid[33];

// بعد از یک هولد روی یک آیتم لیست، LVGL معمولاً یک CLICKED اضافه هم موقع
// رهاکردن انگشت می‌فرستد - این فلگ از اجرای اشتباه منطق تپ جلوگیری می‌کند.
static bool s_list_long_press_handled = false;

// --- منوی Forget/Connect/Cancel برای شبکه‌های شناخته‌شده ---
static lv_obj_t *s_ssid_action_msgbox = NULL;
static char s_context_ssid[33];
static char s_context_password[65];

typedef enum {
    SSID_ACTION_CONNECT,
    SSID_ACTION_FORGET,
    SSID_ACTION_DISCONNECT,
    SSID_ACTION_CANCEL,
} ssid_action_t;

static bool is_already_connected_to(const char *ssid)
{
    return wifi_is_connected() && strcmp(wifi_get_connected_ssid(), ssid) == 0;
}

typedef enum {
    CONNECT_SOURCE_OPEN,        // شبکه‌ی بدون رمز
    CONNECT_SOURCE_AUTO_KNOWN,  // تلاش خودکار با رمز از قبل ذخیره‌شده
    CONNECT_SOURCE_MANUAL,      // رمز تازه از صفحه‌ی کیبورد وارد شده
} connect_source_t;

typedef struct {
    char ssid[33];
    char password[65];
    connect_source_t source;
} connect_task_arg_t;

static void show_list_view(void);
static void show_password_view(const char *ssid);
static void start_scan(void);
static void populate_list(void);
static void ssid_btn_event_cb(lv_event_t *e);
static void ssid_btn_long_press_cb(lv_event_t *e);
static void start_connect(const char *ssid, const char *password, connect_source_t source);


typedef enum {
    NET_OP_DISCONNECT,
    NET_OP_RECONNECT_FROM_LIST,
} net_op_t;

// اول MQTT، بعد Wi-Fi - بلاک‌کننده است پس در تسک جدا اجرا می‌شود
static void net_op_task(void *arg)
{
    net_op_t op = (net_op_t)(uintptr_t)arg;

    mqtt_manager_prepare_for_network_loss();
    vTaskDelay(pdMS_TO_TICKS(150));

    if (op == NET_OP_DISCONNECT) {
        wifi_manager_disconnect();
    } else {
        wifi_manager_reconnect_from_list();
    }
    vTaskDelete(NULL);
}

static void run_net_op(net_op_t op)
{
    xTaskCreate(net_op_task, "net_op", 4096, (void *)(uintptr_t)op, 3, NULL);
}

// ---------------------------------------------------------------------
// ساخت یک آیتم لیست برای یک نتیجه‌ی اسکن (با ایندکس idx در s_scan_results)
// ---------------------------------------------------------------------
static void ssid_action_btn_cb(lv_event_t *e)
{
    ssid_action_t action = (ssid_action_t)(uintptr_t)lv_event_get_user_data(e);

    switch (action) {
        case SSID_ACTION_CONNECT:
            if (is_already_connected_to(s_context_ssid)) {
                lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
                lv_label_set_text(s_list_status_label, "Already connected");
            } else {
                lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
                lv_label_set_text(s_list_status_label, "Connecting...");
                start_connect(s_context_ssid, s_context_password, CONNECT_SOURCE_AUTO_KNOWN);
            }
            break;

        case SSID_ACTION_DISCONNECT :
            run_net_op(NET_OP_DISCONNECT);            lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
            lv_label_set_text(s_list_status_label, "Disconnected");
            populate_list(); // به‌روزرسانی لیست برای حذف هایلایت آبی
            break;


        case SSID_ACTION_FORGET: {
            bool was_connected_to_this = is_already_connected_to(s_context_ssid);

            wifi_config_remove(s_context_ssid);
            for (int i = 0; i < s_scan_count; i++) {
                if (strcmp(s_scan_results[i].ssid, s_context_ssid) == 0) {
                    s_scan_is_known[i] = false;
                    break;
                }
            }

            if (was_connected_to_this) {
                // این شبکه دیگر معتبر نیست - قطعش کن و سعی کن به بهترین گزینه‌ی
                // باقی‌مانده در لیست شناخته‌شده وصل شو
                run_net_op(NET_OP_RECONNECT_FROM_LIST);            }

            populate_list();
            break;
        }

        case SSID_ACTION_CANCEL:
        default:
            break;
    }

    if (s_ssid_action_msgbox != NULL) {
        lv_msgbox_close(s_ssid_action_msgbox);
        s_ssid_action_msgbox = NULL;
    }
}

static void ssid_btn_long_press_cb(lv_event_t *e)
{
    s_list_long_press_handled = true;
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_scan_count) {
        return;
    }

    strncpy(s_context_ssid, s_scan_results[idx].ssid, sizeof(s_context_ssid) - 1);
    s_context_ssid[sizeof(s_context_ssid) - 1] = '\0';
    
    if (s_scan_is_known[idx]) {
        strncpy(s_context_password, s_scan_known_password[idx], sizeof(s_context_password) - 1);
        s_context_password[sizeof(s_context_password) - 1] = '\0';
    } else {
        s_context_password[0] = '\0';
    }

    bool is_connected = is_already_connected_to(s_context_ssid);
    bool is_known = s_scan_is_known[idx];

    s_ssid_action_msgbox = lv_msgbox_create(NULL);
    lv_obj_t *backdrop = lv_obj_get_parent(s_ssid_action_msgbox);
    lv_obj_set_style_bg_color(backdrop, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(backdrop, LV_OPA_80, 0);

    lv_msgbox_add_title(s_ssid_action_msgbox, s_context_ssid);

    // --- منطق نمایش دکمه‌ها بر اساس وضعیت ---
    if (is_connected) {
        lv_msgbox_add_text(s_ssid_action_msgbox, "Currently connected to this network.");
        
        lv_obj_t *btn_disconnect = lv_msgbox_add_footer_button(s_ssid_action_msgbox, "Disconnect");
        lv_obj_add_event_cb(btn_disconnect, ssid_action_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)SSID_ACTION_DISCONNECT);
    } else if (is_known) {
        lv_msgbox_add_text(s_ssid_action_msgbox, "This network is saved.");
        
        lv_obj_t *btn_connect = lv_msgbox_add_footer_button(s_ssid_action_msgbox, "Connect");
        lv_obj_add_event_cb(btn_connect, ssid_action_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)SSID_ACTION_CONNECT);
    } else {
        lv_msgbox_add_text(s_ssid_action_msgbox, "New network.");
        
        lv_obj_t *btn_connect = lv_msgbox_add_footer_button(s_ssid_action_msgbox, "Connect");
        lv_obj_add_event_cb(btn_connect, ssid_action_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)SSID_ACTION_CONNECT);
    }

    // دکمه Forget فقط اگر متصل باشیم یا شبکه ذخیره شده باشد نمایش داده می‌شود
    if (is_connected || is_known) {
        lv_obj_t *btn_forget = lv_msgbox_add_footer_button(s_ssid_action_msgbox, "Forget");
        lv_obj_add_event_cb(btn_forget, ssid_action_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)SSID_ACTION_FORGET);
    }

    // دکمه Cancel همیشه نمایش داده می‌شود
    lv_obj_t *btn_cancel = lv_msgbox_add_footer_button(s_ssid_action_msgbox, "Cancel");
    lv_obj_add_event_cb(btn_cancel, ssid_action_btn_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)SSID_ACTION_CANCEL);
}

static void add_list_item(int idx, bool highlight_connected)
{
    char buf[48];
    const char *lock_mark = (s_scan_results[idx].authmode == WIFI_AUTH_OPEN) ? "" : LV_SYMBOL_SETTINGS " ";
    snprintf(buf, sizeof(buf), "%s%s (%d dBm)", lock_mark, s_scan_results[idx].ssid, s_scan_results[idx].rssi);

    lv_obj_t *btn = lv_list_add_button(s_list, LV_SYMBOL_WIFI, buf);
    lv_obj_add_event_cb(btn, ssid_btn_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);
    lv_obj_add_event_cb(btn, ssid_btn_long_press_cb, LV_EVENT_LONG_PRESSED, (void *)(uintptr_t)idx);

    if (highlight_connected) {
        lv_obj_set_style_bg_color(btn, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
    }
}

// ---------------------------------------------------------------------
// بازسازی کامل لیست از روی s_scan_results فعلی (بدون نیاز به اسکن مجدد) -
// شبکه‌ای که هم‌اکنون به آن وصلیم (در صورت وجود بین نتایج) آبی و در ابتدای
// لیست نشان داده می‌شود.
// ---------------------------------------------------------------------
static void populate_list(void)
{
    lv_obj_clean(s_list);

    if (s_scan_count == 0) {
        lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
        lv_label_set_text(s_list_status_label, "No networks found");
        return;
    }

    bool is_connected = wifi_is_connected();
    const char *connected_ssid = wifi_get_connected_ssid();

    int connected_idx = -1;
    if (is_connected) {
        for (int i = 0; i < s_scan_count; i++) {
            if (strcmp(s_scan_results[i].ssid, connected_ssid) == 0) {
                connected_idx = i;
                break;
            }
        }
    }

    if (connected_idx >= 0) {
        add_list_item(connected_idx, true);
    }
    for (int i = 0; i < s_scan_count; i++) {
        if (i == connected_idx) {
            continue;
        }
        add_list_item(i, false);
    }
}

// ---------------------------------------------------------------------
// تسک پس‌زمینه‌ی اسکن - چون wifi_manager_scan بلاک‌کننده است، هرگز از
// تسک LVGL صدا زده نمی‌شود.
// ---------------------------------------------------------------------
static void scan_task(void *arg)
{
    int n = wifi_manager_scan(s_scan_results, WIFI_SETUP_SCAN_MAX);

    wifi_known_network_t known[WIFI_CONFIG_MAX_NETWORKS];
    int known_count = wifi_config_get_all(known, WIFI_CONFIG_MAX_NETWORKS);

    for (int i = 0; i < n; i++) {
        s_scan_is_known[i] = false;
        for (int k = 0; k < known_count; k++) {
            if (strcmp(s_scan_results[i].ssid, known[k].ssid) == 0) {
                s_scan_is_known[i] = true;
                strncpy(s_scan_known_password[i], known[k].password, sizeof(s_scan_known_password[i]) - 1);
                s_scan_known_password[i][sizeof(s_scan_known_password[i]) - 1] = '\0';
                break;
            }
        }
    }

    lcd_driver_lvgl_lock();
    s_scan_count = n;
    populate_list();
    lcd_driver_lvgl_unlock();
    vTaskDelete(NULL);
}

static void start_scan(void)
{
    lv_obj_clean(s_list);
    lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
    lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
    lv_label_set_text(s_list_status_label, "Scanning networks...");

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
static void connect_task(void *arg)
{
    connect_task_arg_t *a = (connect_task_arg_t *)arg;
    esp_err_t ret = wifi_manager_connect_and_save(a->ssid, a->password);

    lcd_driver_lvgl_lock();

    if (ret == ESP_OK) {

        // ✅ به‌روزرسانی آرایه‌های محلی تا بدون اسکن مجدد، شبکه "شناخته‌شده" محسوب شود
        for (int i = 0; i < s_scan_count; i++) {
            if (strcmp(s_scan_results[i].ssid, a->ssid) == 0) {
                s_scan_is_known[i] = true;
                strncpy(s_scan_known_password[i], a->password, sizeof(s_scan_known_password[i]) - 1);
                s_scan_known_password[i][sizeof(s_scan_known_password[i]) - 1] = '\0';
                break;
            }
        }
        // ⚠️ طبق درخواست: بعد از اتصال موفق، از صفحه‌ی تنظیمات خارج نمی‌شویم -
        // فقط به لیست برمی‌گردیم و هایلایت آبی/بالای‌لیست را به‌روز می‌کنیم.
        show_list_view();
        populate_list();
        lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(s_list_status_label, lv_palette_main(LV_PALETTE_GREEN), 0);
        char msg[48];
        snprintf(msg, sizeof(msg), "Connected to %s", a->ssid);
        lv_label_set_text(s_list_status_label, msg);
    } else {
        switch (a->source) {
            case CONNECT_SOURCE_AUTO_KNOWN:
                // رمز ذخیره‌شده دیگر معتبر نیست (مثلاً کاربر رمز روتر را عوض کرده) -
                // از NVS حذفش کن و دوباره از کاربر رمز تازه بخواه.
                wifi_config_remove(a->ssid);
                for (int i = 0; i < s_scan_count; i++) {
                    if (strcmp(s_scan_results[i].ssid, a->ssid) == 0) {
                        s_scan_is_known[i] = false;
                        break;
                    }
                }
                show_password_view(a->ssid);
                lv_obj_set_style_text_color(s_pass_status_label, lv_palette_main(LV_PALETTE_RED), 0);
                lv_label_set_text(s_pass_status_label, "Old password no longer valid\nenter new password");
                break;

            case CONNECT_SOURCE_MANUAL:
                // در همون صفحه‌ی کیبورد بمون، اجازه بده کاربر دوباره امتحان کند
                lv_obj_clear_flag(s_pass_keyboard, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_text_color(s_pass_status_label, lv_palette_main(LV_PALETTE_RED), 0);
                lv_label_set_text(s_pass_status_label, "Connection failed\ncheck password");
                break;

            case CONNECT_SOURCE_OPEN:
            default:
                show_list_view();
                lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
                lv_obj_set_style_text_color(s_list_status_label, lv_palette_main(LV_PALETTE_RED), 0);
                lv_label_set_text(s_list_status_label, "Connection failed");
                break;
        }
    }

    lcd_driver_lvgl_unlock();

    free(a);
    vTaskDelete(NULL);
}

static void start_connect(const char *ssid, const char *password, connect_source_t source)
{
    connect_task_arg_t *arg = malloc(sizeof(connect_task_arg_t));
    if (arg == NULL) {
        return;
    }
    strncpy(arg->ssid, ssid, sizeof(arg->ssid) - 1);
    arg->ssid[sizeof(arg->ssid) - 1] = '\0';
    strncpy(arg->password, password, sizeof(arg->password) - 1);
    arg->password[sizeof(arg->password) - 1] = '\0';
    arg->source = source;

    xTaskCreate(connect_task, "wifi_connect_task", 4096, arg, 3, NULL);
}

static void ssid_btn_event_cb(lv_event_t *e)
{
    if (s_list_long_press_handled) {
        s_list_long_press_handled = false;
        return;
    }

    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    if (idx < 0 || idx >= s_scan_count) {
        return;
    }

    strncpy(s_selected_ssid, s_scan_results[idx].ssid, sizeof(s_selected_ssid) - 1);
    s_selected_ssid[sizeof(s_selected_ssid) - 1] = '\0';

    if (is_already_connected_to(s_selected_ssid)) {
        lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
        lv_label_set_text(s_list_status_label, "Already connected");
        return;
    }

    if (s_scan_results[idx].authmode == WIFI_AUTH_OPEN) {
        start_connect(s_selected_ssid, "", CONNECT_SOURCE_OPEN);
    } else if (s_scan_is_known[idx]) {
        // این شبکه قبلاً وصل شده - با رمز ذخیره‌شده، بدون نمایش صفحه‌ی کیبورد، مستقیم تلاش کن
        lv_obj_clear_flag(s_list_status_label, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_text_color(s_list_status_label, lv_color_white(), 0);
        lv_label_set_text(s_list_status_label, "Connecting...");
        start_connect(s_selected_ssid, s_scan_known_password[idx], CONNECT_SOURCE_AUTO_KNOWN);
    } else {
        show_password_view(s_selected_ssid);
    }
}

static void keyboard_event_cb(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_READY) {
        return;
    }

    const char *password = lv_textarea_get_text(s_pass_textarea);
    start_connect(s_selected_ssid, password, CONNECT_SOURCE_MANUAL);

    lv_obj_set_style_text_color(s_pass_status_label, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_label_set_text(s_pass_status_label, "Connecting...");
    lv_obj_add_flag(s_pass_keyboard, LV_OBJ_FLAG_HIDDEN);   // تا پایان تلاش، اجازه‌ی ورود دوباره نده
}

static void pass_eye_btn_event_cb(lv_event_t *e)
{
    s_pass_visible = !s_pass_visible;
    lv_textarea_set_password_mode(s_pass_textarea, !s_pass_visible);
    lv_obj_t *icon = lv_obj_get_child(s_pass_eye_btn, 0);
    lv_label_set_text(icon, s_pass_visible ? LV_SYMBOL_EYE_CLOSE : LV_SYMBOL_EYE_OPEN);
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
    snprintf(title, sizeof(title), "Password for %s", ssid);
    lv_label_set_text(s_pass_title, title);

    lv_textarea_set_text(s_pass_textarea, "");
    lv_label_set_text(s_pass_status_label, "");

    // هر بار که صفحه‌ی رمز از نو باز می‌شود، رمز مخفی شروع شود
    s_pass_visible = false;
    lv_textarea_set_password_mode(s_pass_textarea, true);
    lv_label_set_text(lv_obj_get_child(s_pass_eye_btn, 0), LV_SYMBOL_EYE_OPEN);
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
    lv_label_set_text(s_list_title, "Select WiFi Network");
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
    lv_label_set_text(s_pass_title, "Network Password");
    lv_obj_align(s_pass_title, LV_ALIGN_TOP_MID, 0, 15);

    s_pass_textarea = lv_textarea_create(s_pass_view);
    lv_textarea_set_password_mode(s_pass_textarea, true);
    lv_textarea_set_one_line(s_pass_textarea, true);
    lv_textarea_set_max_length(s_pass_textarea, 64);
    lv_obj_set_width(s_pass_textarea, 180);
    lv_obj_align(s_pass_textarea, LV_ALIGN_TOP_MID, 0, 55);

    s_pass_eye_btn = lv_button_create(s_pass_view);
    lv_obj_set_size(s_pass_eye_btn, 40, 40);
    lv_obj_align_to(s_pass_eye_btn, s_pass_textarea, LV_ALIGN_OUT_RIGHT_MID, 8, 0);
    lv_obj_add_event_cb(s_pass_eye_btn, pass_eye_btn_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *eye_icon = lv_label_create(s_pass_eye_btn);
    lv_label_set_text(eye_icon, LV_SYMBOL_EYE_OPEN);
    lv_obj_center(eye_icon);

    s_pass_status_label = lv_label_create(s_pass_view);
    lv_obj_set_width(s_pass_status_label, LV_PCT(90));
    lv_obj_set_style_text_align(s_pass_status_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(s_pass_status_label, "");
    lv_obj_align_to(s_pass_status_label, s_pass_textarea, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    s_pass_keyboard = lv_keyboard_create(s_pass_view);
    lv_keyboard_set_textarea(s_pass_keyboard, s_pass_textarea);
    lv_obj_set_size(s_pass_keyboard, LV_PCT(100), 240);
    lv_obj_align(s_pass_keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s_pass_keyboard, keyboard_event_cb, LV_EVENT_READY, NULL);
    // ⚠️ بدون این خط، نگه‌داشتن انگشت روی یک کلید (حتی کمی بیشتر از حد معمول،
    // که با توجه به وضعیت فعلی باس I2C تاچ محتمله) باعث ثبت مکرر همون کاراکتر
    // می‌شود - همون رفتار پیش‌فرض LVGL برای کلید Backspace که این‌جا برای
    // همه‌ی کلیدها غیرفعالش می‌کنیم.
    lv_buttonmatrix_set_button_ctrl_all(s_pass_keyboard, LV_BUTTONMATRIX_CTRL_NO_REPEAT);

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

void wifi_setup_screen_notify_state_change(void)
{
    if (lv_obj_has_flag(s_overlay, LV_OBJ_FLAG_HIDDEN)) {
        return;   // صفحه اصلاً باز نیست
    }
    if (lv_obj_has_flag(s_list_view, LV_OBJ_FLAG_HIDDEN)) {
        return;   // کاربر روی صفحه‌ی کیبورد رمزه، دست نزن
    }
    // هایلایت آبی/بالای‌لیست را با وضعیت واقعی به‌روز کن - بدون نیاز به
    // خروج و ورود دوباره به این صفحه
    populate_list();
}