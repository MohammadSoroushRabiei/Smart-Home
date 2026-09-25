/**
 * @file lv_conf.h
 * @brief پیکربندی LVGL برای دموی PC - عین lv_conf.h شبیه‌ساز (v9.5.0).
 */
#pragma once

#define LV_COLOR_DEPTH 16
#define LV_USE_STDLIB_MALLOC LV_STDLIB_CLIB
#define LV_USE_OS LV_OS_NONE

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "SDL2/SDL.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (SDL_GetTicks())

#define LV_USE_SDL 1

#define LV_USE_QRCODE       1
#define LV_USE_LIST         1
#define LV_USE_MSGBOX       1
#define LV_USE_KEYBOARD     1
#define LV_USE_BUTTONMATRIX 1
#define LV_USE_TEXTAREA     1

#define LV_FONT_MONTSERRAT_28 1
