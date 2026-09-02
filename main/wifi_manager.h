#pragma once

#include <stdbool.h>


typedef enum{
    WIFI_STATE_OFFLINE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED
} wifi_state_t ;

// نوع تابع callback که هر بار وضعیت Wi-Fi تغییر کند، صدا زده می‌شود
typedef void (*wifi_state_change_cb_t)(wifi_state_t new_state);


bool wifi_is_connected(void);

void wifi_init_sta(void);

wifi_state_t wifi_get_state(void);

/**
 * @brief ثبت یک callback برای دریافت اعلان تغییر وضعیت Wi-Fi.
 *        فقط یک callback در این نسخه پشتیبانی می‌شود (برای ساده نگه‌داشتن).
 */
void wifi_register_state_change_cb(wifi_state_change_cb_t cb);

/**
 * @brief تلاش مجدد برای اتصال؛ شمارنده‌ی retry را صفر می‌کند تا دوباره از اول تلاش شود.
 *        برای استفاده در دکمه‌ی "تلاش مجدد" روی LCD یا وب.
 */
void wifi_retry_connect(void);

/**
 * @brief دریافت رشته‌ی IP فعلی (فقط زمانی معتبر است که وضعیت CONNECTED باشد).
 *        اگر متصل نباشد، رشته‌ی خالی برمی‌گرداند.
 */
const char *wifi_get_ip_str(void);