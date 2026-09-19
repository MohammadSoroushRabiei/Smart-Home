#include "time_sync.h"

#include <stdlib.h>
#include "esp_log.h"
#include "esp_netif_sntp.h"

// ایران: +03:30 و از سال ۱۳۹۱ به بعد بدون DST (علامت POSIX برعکس است)
#define APP_TZ        "UTC-3:30"
#define NTP_SERVER    "pool.ntp.org"

// قبل از سپتامبر ۲۰۲۳ قطعاً تنظیم نشده (برای تشخیص بعد از ریست بدون callback)
#define MIN_VALID_EPOCH  1690000000LL

static const char *TAG = "time_sync";

static volatile bool s_synced = false;

static void on_time_synced(struct timeval *tv)
{
    s_synced = true;
    time_t now = time(NULL);
    struct tm tm_info;
    localtime_r(&now, &tm_info);
    ESP_LOGI(TAG, "Time synced via NTP: %04d-%02d-%02d %02d:%02d:%02d",
             tm_info.tm_year + 1900, tm_info.tm_mon + 1, tm_info.tm_mday,
             tm_info.tm_hour, tm_info.tm_min, tm_info.tm_sec);
}

void time_sync_init(void)
{
    setenv("TZ", APP_TZ, 1);
    tzset();

    // اگر ساعت از قبل معتبر باشد (مثلاً پس از soft-reboot با RTC حفظ‌شده)
    if (time(NULL) > (time_t)MIN_VALID_EPOCH) {
        s_synced = true;
    }

    esp_sntp_config_t cfg = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    cfg.sync_cb = on_time_synced;
    esp_netif_sntp_init(&cfg);

    ESP_LOGI(TAG, "SNTP started (server: %s, TZ: %s)", NTP_SERVER, APP_TZ);
}

bool time_sync_is_ready(void)
{
    if (s_synced) {
        return true;
    }
    // fallback: callback ممکن است از دست رفته باشد
    if (time(NULL) > (time_t)MIN_VALID_EPOCH) {
        s_synced = true;
        return true;
    }
    return false;
}
