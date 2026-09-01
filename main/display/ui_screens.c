#include "ui_screens.h"

#include "lvgl.h"
#include "lcd_driver.h"
#include "wifi_manager.h"

static lv_obj_t *s_wifi_status_label;
static lv_obj_t *s_retry_btn;

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

void ui_screens_init(void)
{
    lv_obj_t *scr = lv_screen_active();

    s_wifi_status_label = lv_label_create(scr);
    lv_label_set_text(s_wifi_status_label, "WiFi: Offline");
    lv_obj_align(s_wifi_status_label, LV_ALIGN_TOP_MID, 0, 10);

    s_retry_btn = lv_button_create(scr);
    lv_obj_set_size(s_retry_btn,120,50);
    lv_obj_align(s_retry_btn,LV_ALIGN_TOP_MID,0,45);
    lv_obj_add_event_cb(s_retry_btn,retry_btn_event_cb,LV_EVENT_CLICKED,NULL);

    lv_obj_t * btn_label = lv_label_create(s_retry_btn);
    lv_label_set_text(btn_label,"Retry");
    lv_obj_center(btn_label);

}

void ui_update_wifi_status(wifi_state_t state)
{
    if (s_wifi_status_label == NULL) {
        return;  
    }
    lv_label_set_text(s_wifi_status_label, wifi_state_to_display_text(state));
}