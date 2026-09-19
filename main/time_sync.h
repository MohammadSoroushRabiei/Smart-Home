#pragma once

#include <stdbool.h>
#include <time.h>

/**
 * @brief راه‌اندازی SNTP + تنظیم منطقه‌ی زمانی (ایران، UTC+3:30).
 *        غیربلاک‌کننده است: وقتی WiFi وصل شود خودش ساعت را می‌گیرد و
 *        time_sync_is_ready() فعال می‌شود. باید بعد از wifi_manager_init_radio
 *        (یعنی وقتی esp_netif از قبل ساخته شده) صدا زده شود.
 */
void time_sync_init(void);

/**
 * @brief آیا ساعت با سرور NTP تنظیم شده است؟
 *        تا پیش از sync، ماژول‌های وابسته به ساعت واقعی (ml_agent و
 *        virtual_devices) باید غیرفعال بمانند.
 */
bool time_sync_is_ready(void);
