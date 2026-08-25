#pragma once

#include <stdbool.h>


typedef enum{
    WIFI_STATE_OFFLINE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED
} wifi_state_t ;


bool wifi_is_connected(void);

void wifi_init_sta(void);

wifi_state_t wifi_get_state(void);

