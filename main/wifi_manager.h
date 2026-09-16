#pragma once

#include <stdbool.h>
#include "esp_wifi.h"
#include "esp_err.h"


typedef enum{
    WIFI_STATE_OFFLINE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED
} wifi_state_t ;

// نوع تابع callback که هر بار وضعیت Wi-Fi تغییر کند، صدا زده می‌شود
typedef void (*wifi_state_change_cb_t)(wifi_state_t new_state);

typedef struct {
    char ssid[33];
    int8_t rssi;
    wifi_auth_mode_t authmode;
} wifi_scan_result_t;


bool wifi_is_connected(void);

/**
 * @brief راه‌اندازی یک‌باره‌ی درایور Wi-Fi: netif، event loop، حالت STA، و روشن
 *        کردن رادیو (esp_wifi_start). به‌عمد به هیچ شبکه‌ای وصل نمی‌شود - این کار
 *        وظیفه‌ی wifi_manager_enable است، تا سیستم واقعاً «آفلاین» بوت شود.
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
void wifi_manager_init_radio(void);

wifi_state_t wifi_get_state(void);

/**
 * @brief ثبت یک callback برای دریافت اعلان تغییر وضعیت Wi-Fi.
 *        فقط یک callback در این نسخه پشتیبانی می‌شود (برای ساده نگه‌داشتن).
 */
void wifi_register_state_change_cb(wifi_state_change_cb_t cb);

/**
 * @brief شروع تلاش خودکار برای اتصال، به‌ترتیب از جدیدترین شبکه‌ی شناخته‌شده
 *        (طبق لیست wifi_config، ایندکس ۰ = جدیدترین) تا قدیمی‌ترین.
 *        غیربلاک‌کننده - نتیجه از طریق wifi_state_change_cb_t اعلام می‌شود.
 *        اگر لیست خالی باشد، فقط رادیو روشن می‌ماند (برای اسکن) و وضعیت
 *        در OFFLINE باقی می‌ماند.
 *        اگر رادیو قبلاً خاموش شده بود (بعد از wifi_manager_disable)، دوباره
 *        روشنش می‌کند.
 */
void wifi_manager_enable(void);

/**
 * @brief قطع دستی توسط کاربر: قطع اتصال فعلی (اگر بود) و خاموش کردن کامل
 *        رادیو. هیچ تلاش خودکاری تا فراخوانی بعدی wifi_manager_enable
 *        انجام نمی‌شود.
 */
void wifi_manager_disable(void);

/**
 * @brief اسکن شبکه‌های اطراف.
 *        ⚠️ این تابع بلاک‌کننده است (معمولاً ۲-۴ ثانیه طول می‌کشد) - همیشه از
 *        یک تسک جداگانه صدا بزنید، هرگز از تسک LVGL.
 * @param out         آرایه‌ی خروجی
 * @param max_results حداکثر تعداد قابل‌نوشتن در out
 * @return تعداد شبکه‌های یافت‌شده (بدون SSID تکراری، مرتب‌شده نزولی بر اساس RSSI)
 */
int wifi_manager_scan(wifi_scan_result_t *out, int max_results);

/**
 * @brief اتصال دستی به یک SSID/پسورد مشخص (معمولاً از صفحه‌ی تنظیمات وای‌فای).
 *        ⚠️ این تابع بلاک‌کننده است (تا حدود ۱۵ ثانیه، تا مشخص شدن نتیجه) -
 *        همیشه از یک تسک جداگانه صدا بزنید، هرگز از تسک LVGL.
 *        روی موفقیت، این شبکه به‌طور خودکار در لیست MRU (wifi_config) ذخیره
 *        می‌شود.
 * @return ESP_OK روی موفقیت، ESP_FAIL روی شکست یا timeout
 */
esp_err_t wifi_manager_connect_and_save(const char *ssid, const char *password);

/**
 * @brief دریافت رشته‌ی IP فعلی (فقط زمانی معتبر است که وضعیت CONNECTED باشد).
 *        اگر متصل نباشد، رشته‌ی خالی برمی‌گرداند.
 */
const char *wifi_get_ip_str(void);

/**
 * @brief دریافت SSID شبکه‌ای که هم‌اکنون به آن متصلیم.
 *        فقط زمانی معتبر است که wifi_is_connected() برابر true باشد.
 */
const char *wifi_get_connected_ssid(void);