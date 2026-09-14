#pragma once

#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define WIFI_CONFIG_MAX_NETWORKS   3
#define WIFI_CONFIG_SSID_MAX_LEN   32   // حداکثر طول SSID طبق استاندارد 802.11
#define WIFI_CONFIG_PASS_MAX_LEN   64   // حداکثر طول پسورد WPA2-PSK

typedef struct {
    char ssid[WIFI_CONFIG_SSID_MAX_LEN + 1];
    char password[WIFI_CONFIG_PASS_MAX_LEN + 1];
} wifi_known_network_t;

/**
 * @brief راه‌اندازی ماژول: باز کردن NVS namespace و ساخت mutex.
 *        باید یک‌بار در ابتدای app_main صدا زده شود (قبل از هر تابع دیگر این فایل).
 */
esp_err_t wifi_config_init(void);

/**
 * @brief دریافت لیست شبکه‌های شناخته‌شده به ترتیب MRU
 *        (ایندکس ۰ = آخرین شبکه‌ای که با موفقیت بهش وصل شده‌ایم).
 * @param out       آرایه‌ی خروجی که پر می‌شود
 * @param max_count حداکثر تعداد قابل‌نوشتن در out (معمولاً WIFI_CONFIG_MAX_NETWORKS)
 * @return تعداد واقعی شبکه‌های ذخیره‌شده (می‌تواند کمتر از max_count یا حتی صفر باشد)
 */
int wifi_config_get_all(wifi_known_network_t *out, int max_count);

/**
 * @brief یک شبکه را به بالای لیست MRU منتقل می‌کند (یا در صورت نبودن در لیست، به آن اضافه می‌کند).
 *        اگر لیست پر بود، قدیمی‌ترین ورودی حذف می‌شود.
 *        باید بعد از هر اتصال موفق (چه خودکار، چه از صفحه‌ی تنظیمات) صدا زده شود.
 */
esp_err_t wifi_config_promote(const char *ssid, const char *password);

/**
 * @brief پاک کردن کامل لیست شبکه‌های ذخیره‌شده.
 */
esp_err_t wifi_config_clear_all(void);

#ifdef __cplusplus
}
#endif