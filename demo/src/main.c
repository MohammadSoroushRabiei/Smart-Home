/**
 * @file main.c
 * @brief دموی ارائه: همان ترتیب app_main برد، فقط سخت‌افزار مجازی.
 *
 * واقعی و دست‌نخورده اجرا می‌شود: app_state، ml_agent (+ مدل واقعی)،
 * virtual_devices، led/lock (روی GPIO مجازی) و تمام UI.
 * شبیه‌سازی می‌شود: WiFi، بروکر MQTT، سنسور BME280، دیتابیس چهره، NVS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <SDL2/SDL.h>

#include "lvgl.h"

#include "app_state.h"
#include "ml_agent.h"
#include "virtual_devices.h"
#include "time_sync.h"
#include "password_manager.h"
#include "enroll_token.h"
#include "face_db.h"
#include "attendance.h"
#include "wifi_manager.h"
#include "mqtt_manager.h"
#include "mqtt_config.h"
#include "wifi_config.h"
#include "lcd_driver.h"

#include "ui_screens.h"
#include "keypad_screen.h"
#include "wifi_setup_screen.h"
#include "setting_screen.h"
#include "mqtt_setup_screen.h"
#include "attendance_screen.h"
#include "led.h"
#include "lock.h"

#include "scenario.h"

static volatile int s_quit = 0;

#if SDL_VERSION_ATLEAST(3, 0, 0)
#define SIM_QUIT_EVENT SDL_EVENT_QUIT
#else
#define SIM_QUIT_EVENT SDL_QUIT
#endif

static int quit_watch(void *ud, SDL_Event *e)
{
    (void)ud;
    if (e->type == SIM_QUIT_EVENT) {
        s_quit = 1;
    }
    return 1;
}

// ---- همان کال‌بک‌های main.c برد - وضعیت شبکه را به UI می‌رسانند ----

static void on_wifi_state_change(wifi_state_t state)
{
    lcd_driver_lvgl_lock();
    ui_update_wifi_status(state);
    wifi_setup_screen_notify_state_change();

    if (state == WIFI_STATE_CONNECTED) {
        mqtt_manager_notify_network_available();
        const char *ip = wifi_get_ip_str();
        char url[48];
        snprintf(url, sizeof(url), "https://%s/recognize", ip);
        keypad_screen_update_capture_qr(url);

        char dash_url[40];
        snprintf(dash_url, sizeof(dash_url), "https://%s/", ip);
        ui_update_dashboard_qr(dash_url);
    } else {
        mqtt_manager_notify_network_lost();
        keypad_screen_update_capture_qr("");
        ui_update_dashboard_qr("");
    }
    lcd_driver_lvgl_unlock();
}

static void on_mqtt_state_change(mqtt_manager_state_t state)
{
    lcd_driver_lvgl_lock();
    ui_update_mqtt_status(state);
    mqtt_setup_screen_notify_state_change(state);
    lcd_driver_lvgl_unlock();
}

static void usage(void)
{
    printf("Smart Home LCD Demo (virtual hardware)\n"
           "  --demo          auto-play scenario on start (lv_demo_music style)\n"
           "  --zoom FACTOR   window scale (default 1x; F = fullscreen)\n"
           "keys: F fullscreen | D start scenario\n");
}

int main(int argc, char **argv)
{
    int zoom = 1;
    int autoplay = 0;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--demo") == 0) {
            autoplay = 1;
        } else if (strcmp(argv[i], "--zoom") == 0 && i + 1 < argc) {
            zoom = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage();
            return 0;
        } else {
            fprintf(stderr, "unknown arg: %s\n", argv[i]);
            usage();
            return 1;
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_AddEventWatch(quit_watch, NULL);

    lv_init();

    lv_display_t *disp = lv_sdl_window_create(320, 480);
    if (disp == NULL) {
        fprintf(stderr, "failed to create SDL window\n");
        return 1;
    }
    if (zoom > 1) {
        // پنجره‌ی بزرگ‌تر از ابتدا (resize دستی هم همیشه ممکن است)
        SDL_SetWindowSize(SDL_GetWindowFromID(1), 320 * zoom, 480 * zoom);
    }
    lv_sdl_mouse_create();

    // ===== همان ترتیب app_main برد، فقط سخت‌افزار مجازی =====
    app_state_init();

    wifi_config_init();
    mqtt_config_init();

    wifi_manager_init_radio();

    time_sync_init();

    password_manager_init();
    enroll_token_init();
    face_db_init();
    attendance_init();

    lcd_driver_init();
    app_state_set_lcd_available(true);

    lcd_driver_lvgl_lock();
    ui_screens_init();
    keypad_screen_init();
    wifi_setup_screen_init();
    settings_screen_init();
    mqtt_setup_screen_init();
    attendance_screen_init();
    lcd_driver_lvgl_unlock();

    led_init();
    lock_init();

    ml_agent_init();

    wifi_register_state_change_cb(on_wifi_state_change);
    wifi_manager_enable();

    mqtt_manager_register_state_change_cb(on_mqtt_state_change);
    mqtt_manager_init();

    virtual_devices_start();

    demo_sensor_feeder_start();
    if (autoplay) {
        demo_scenario_start();
    }

    printf("demo ready - keys: F fullscreen, D start scenario | quit: window close\n");

    while (!s_quit) {
        // دقیقاً مثل lvgl_port_task برد: رندر هم زیر قفل LVGL — بدون این
        // قفل، ترد‌های بک‌اند (WiFi/ML/سنسور) همزمان با رندر race می‌دهند
        lcd_driver_lvgl_lock();
        lv_timer_handler();
        lcd_driver_lvgl_unlock();
        usleep(5000);
    }
    return 0;
}
