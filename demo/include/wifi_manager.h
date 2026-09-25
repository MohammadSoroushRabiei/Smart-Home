/**
 * @file wifi_manager.h
 * @brief شیم PC جایگزین هدر برد (نسخه‌ی برد esp_wifi.h را include می‌کند).
 *        یک وای‌فای «مجازی» کامل: دنباله‌ی اتصال واقعی، اسکن با شبکه‌های
 *        ساختگی، اتصال موفق از صفحه‌ی setup - همه در demo/src/shims.c.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    WIFI_STATE_OFFLINE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED
} wifi_state_t;

typedef enum {
    WIFI_AUTH_OPEN = 0,
    WIFI_AUTH_WPA_PSK,
    WIFI_AUTH_WPA2_PSK
} wifi_auth_mode_t;

typedef void (*wifi_state_change_cb_t)(wifi_state_t new_state);

typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;

bool wifi_is_connected(void);
void wifi_manager_init_radio(void);
wifi_state_t wifi_get_state(void);
void wifi_register_state_change_cb(wifi_state_change_cb_t cb);
void wifi_manager_enable(void);
void wifi_manager_reconnect_from_list(void);
void wifi_manager_disable(void);
void wifi_manager_disconnect(void);
int wifi_manager_scan(wifi_scan_result_t *out, int max_results);
esp_err_t wifi_manager_connect_and_save(const char *ssid, const char *password);
const char *wifi_get_ip_str(void);
const char *wifi_get_connected_ssid(void);
