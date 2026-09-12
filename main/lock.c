#include "lock.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "lock";
static bool s_unlocked = false;
static esp_timer_handle_t s_failsafe_timer = NULL;

static inline void apply_relay_level(bool unlocked)
{
    // Fail-Secure: باز کردن یعنی رله باید انرژایز (فعال) شود
    bool energize = unlocked;
    int level = LOCK_RELAY_ACTIVE_LOW ? (energize ? 0 : 1) : (energize ? 1 : 0);
    gpio_set_level(LOCK_RELAY_PIN, level);
}

static void failsafe_timer_cb(void *arg)
{
    // اگر این تابع اجرا شد یعنی چیزی غیرعادی رخ داده: یک مسیر دیگر
    // (نه app_state) قفل را باز کرده و کسی به‌موقع نبسته‌ش. این فقط
    // یک شبکه‌ی ایمنی آخر برای بستن فیزیکی قفل است - بقیه‌ی سیستم
    // (MQTT/LCD) از این طریق مطلع نمی‌شود، چون این اتفاق نباید بیفتد
    // و اگر افتاد، باید به‌عنوان یک باگ در جای دیگری بررسی شود.
    ESP_LOGW(TAG, "Hardware fail-safe timer fired - lock was not closed via app_state in time!");
    lock_set(false);
}

void lock_init(void)
{
    gpio_reset_pin(LOCK_RELAY_PIN);
    gpio_set_direction(LOCK_RELAY_PIN, GPIO_MODE_OUTPUT);

    // اطمینان از اینکه در بوت، رله غیرفعال و قفل بسته است
    apply_relay_level(false);
    s_unlocked = false;

    const esp_timer_create_args_t timer_args = {
        .callback = &failsafe_timer_cb,
        .name = "lock_failsafe",
    };
    esp_timer_create(&timer_args, &s_failsafe_timer);

    ESP_LOGI(TAG, "Lock driver initialized (pin=%d, active_%s, fail-secure, hw failsafe=%dms)",
             LOCK_RELAY_PIN, LOCK_RELAY_ACTIVE_LOW ? "low" : "high", LOCK_HW_FAILSAFE_MS);
}

void lock_set(bool unlocked)
{
    apply_relay_level(unlocked);
    s_unlocked = unlocked;
    ESP_LOGI(TAG, "Lock %s", unlocked ? "UNLOCKED" : "LOCKED");

    if (s_failsafe_timer) {
        esp_timer_stop(s_failsafe_timer); // اگر در حال اجرا نبود، خطا بی‌ضرر است
        if (unlocked) {
            esp_timer_start_once(s_failsafe_timer, (uint64_t)LOCK_HW_FAILSAFE_MS * 1000);
        }
    }
}

bool lock_is_unlocked(void)
{
    return s_unlocked;
}