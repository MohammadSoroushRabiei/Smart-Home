#include "ui_screens.h"

#include "lvgl.h"
#include "lcd_driver.h"

static lv_obj_t *s_wifi_status_label;

static const char *wifi_state_to_display_text(wifi_state_t state)
{
    switch (state) {
        case WIFI_STATE_OFFLINE:    return "WiFi: Offline";
        case WIFI_STATE_CONNECTING: return "WiFi: Connecting...";
        case WIFI_STATE_CONNECTED:  return "WiFi: Connected";
        default:                    return "WiFi: Unknown";
    }
}

void ui_screens_init(void)
{
    lv_obj_t *scr = lv_screen_active();

    s_wifi_status_label = lv_label_create(scr);
    lv_label_set_text(s_wifi_status_label, "WiFi: Offline");
    lv_obj_align(s_wifi_status_label, LV_ALIGN_TOP_MID, 0, 10);

    // بقیه‌ی عناصر UI (چراغ، قفل، دما/رطوبت) در گام‌های بعدی همین‌جا اضافه می‌شوند
}

void ui_update_wifi_status(wifi_state_t state)
{
    if (s_wifi_status_label == NULL) {
        return;   // هنوز UI ساخته نشده
    }
    lv_label_set_text(s_wifi_status_label, wifi_state_to_display_text(state));
}