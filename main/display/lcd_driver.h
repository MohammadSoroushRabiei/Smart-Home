#pragma once

#include <stdbool.h>
#include "lvgl.h"

// ===== پین‌بندی ثابت LCD (طبق سخت‌افزار پروژه) =====
#define LCD_PIN_D0          4
#define LCD_PIN_D1          5
#define LCD_PIN_D2          6
#define LCD_PIN_D3          7
#define LCD_PIN_D4          15
#define LCD_PIN_D5          16
#define LCD_PIN_D6          17
#define LCD_PIN_D7          18
#define LCD_PIN_WR          8
#define LCD_PIN_DC          9
#define LCD_PIN_CS          10
#define LCD_PIN_RESET       11
#define LCD_PIN_BACKLIGHT   12

// ===== مشخصات پنل =====
#define LCD_H_RES           320
#define LCD_V_RES           480
#define LCD_PIXEL_CLOCK_HZ  20000000

/**
 * @brief راه‌اندازی کامل LCD: باس i80، درایور ST7796، بک‌لایت، و LVGL.
 *        باید فقط یک‌بار و در ابتدای app_main صدا زده شود.
 */
bool lcd_driver_init(void);

/**
 * @brief دریافت handle نمایشگر LVGL برای استفاده در ماژول‌های دیگر (مثلاً UI یا Touch).
 */
lv_display_t *lcd_driver_get_display(void);

/**
 * @brief روشن/خاموش کردن بک‌لایت.
 */
void lcd_backlight_set(bool on);

/**
 * @brief قفل کردن دسترسی به API های LVGL قبل از فراخوانی از یک Task دیگر
 *        (چون LVGL thread-safe نیست). بعد از اتمام کار، حتماً lcd_driver_lvgl_unlock را صدا بزنید.
 */
void lcd_driver_lvgl_lock(void);
void lcd_driver_lvgl_unlock(void);