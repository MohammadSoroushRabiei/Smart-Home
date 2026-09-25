/**
 * @file shims.c
 * @brief سخت‌افزار و بک‌اند مجازی برای دموی ارائه.
 *
 * فلسفه: منطق واقعی فریمور (app_state، ml_agent، virtual_devices، led، lock و
 * تمام UI) دست‌نخورده روی PC اجرا می‌شود و فقط «سخت‌افزار» اینجا شبیه‌سازی
 * می‌شود: GPIO (لاگ)، WiFi مجازی با دنباله‌ی اتصال و اسکنِ ساختگی، بروکر MQTT
 * مجازی که publish ها را لاگ می‌کند، NVS در حافظه، سنسورها و چهره‌ی مجازی.
 */
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "driver/gpio.h"
#include "nvs.h"

#include "lvgl.h"
#include "lcd_driver.h"
#include "wifi_manager.h"
#include "wifi_config.h"
#include "mqtt_manager.h"
#include "time_sync.h"
#include "password_manager.h"
#include "enroll_token.h"
#include "face_db.h"
#include "face_recognition.h"
#include "attendance.h"

void demo_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fputs("[demo] ", stdout);
    vprintf(fmt, ap);
    putchar('\n');
    va_end(ap);
    fflush(stdout);
}

// =====================================================================
// esp_random
// =====================================================================
uint32_t esp_random(void)
{
    static int seeded = 0;
    if (!seeded) {
        srand((unsigned)time(NULL));
        seeded = 1;
    }
    return ((uint32_t)rand() << 16) ^ (uint32_t)rand();
}

// =====================================================================
// esp_timer - تایمرهای نرم‌افزاری روی یک تیک‌تر مشترک
// =====================================================================
#define MAX_TIMERS 16

typedef struct {
    bool used;
    void (*cb)(void *arg);
    void *arg;
    const char *name;
    int64_t next_us;
    uint64_t period_us;        // 0 = one-shot
    bool running;
} demo_timer_t;

static demo_timer_t s_timers[MAX_TIMERS];
static pthread_mutex_t s_timer_mtx = PTHREAD_MUTEX_INITIALIZER;

// note: esp_timer_create_args_t در هدر شیم فقط callback/arg/name دارد
// (سازگار با designated initializer های کد برد)

int64_t esp_timer_get_time(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int64_t)ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                           esp_timer_handle_t *out)
{
    pthread_mutex_lock(&s_timer_mtx);
    for (int i = 0; i < MAX_TIMERS; i++) {
        if (!s_timers[i].used) {
            s_timers[i] = (demo_timer_t){0};
            s_timers[i].used = true;
            s_timers[i].cb = args->callback;
            s_timers[i].arg = args->arg;
            s_timers[i].name = args->name;
            *out = (esp_timer_handle_t)(intptr_t)(i + 1);
            pthread_mutex_unlock(&s_timer_mtx);
            return ESP_OK;
        }
    }
    pthread_mutex_unlock(&s_timer_mtx);
    return ESP_ERR_NO_MEM;
}

static demo_timer_t *timer_by_handle(esp_timer_handle_t h)
{
    intptr_t idx = (intptr_t)h - 1;
    if (idx < 0 || idx >= MAX_TIMERS || !s_timers[idx].used) {
        return NULL;
    }
    return &s_timers[idx];
}

esp_err_t esp_timer_start_once(esp_timer_handle_t h, uint64_t us)
{
    pthread_mutex_lock(&s_timer_mtx);
    demo_timer_t *t = timer_by_handle(h);
    if (t) {
        t->next_us = esp_timer_get_time() + (int64_t)us;
        t->period_us = 0;
        t->running = true;
    }
    pthread_mutex_unlock(&s_timer_mtx);
    return t ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t esp_timer_start_periodic(esp_timer_handle_t h, uint64_t us)
{
    pthread_mutex_lock(&s_timer_mtx);
    demo_timer_t *t = timer_by_handle(h);
    if (t) {
        t->next_us = esp_timer_get_time() + (int64_t)us;
        t->period_us = us;
        t->running = true;
    }
    pthread_mutex_unlock(&s_timer_mtx);
    return t ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t esp_timer_stop(esp_timer_handle_t h)
{
    pthread_mutex_lock(&s_timer_mtx);
    demo_timer_t *t = timer_by_handle(h);
    if (t) {
        t->running = false;
    }
    pthread_mutex_unlock(&s_timer_mtx);
    return t ? ESP_OK : ESP_ERR_INVALID_ARG;
}

esp_err_t esp_timer_delete(esp_timer_handle_t h)
{
    pthread_mutex_lock(&s_timer_mtx);
    demo_timer_t *t = timer_by_handle(h);
    if (t) {
        t->used = false;
    }
    pthread_mutex_unlock(&s_timer_mtx);
    return t ? ESP_OK : ESP_ERR_INVALID_ARG;
}

static void *timer_tick_thread(void *arg)
{
    (void)arg;
    for (;;) {
        int64_t now = esp_timer_get_time();
        pthread_mutex_lock(&s_timer_mtx);
        for (int i = 0; i < MAX_TIMERS; i++) {
            demo_timer_t *t = &s_timers[i];
            if (t->used && t->running && now >= t->next_us) {
                void (*cb)(void *) = t->cb;
                void *cb_arg = t->arg;
                if (t->period_us > 0) {
                    t->next_us = now + (int64_t)t->period_us;
                } else {
                    t->running = false;
                }
                pthread_mutex_unlock(&s_timer_mtx);
                cb(cb_arg);
                pthread_mutex_lock(&s_timer_mtx);
                now = esp_timer_get_time();
            }
        }
        pthread_mutex_unlock(&s_timer_mtx);
        usleep(10 * 1000);
    }
    return NULL;
}

__attribute__((constructor)) static void demo_timers_start(void)
{
    pthread_t th;
    pthread_create(&th, NULL, timer_tick_thread, NULL);
    pthread_detach(th);
}

// =====================================================================
// FreeRTOS → pthread
// =====================================================================
typedef struct {
    TaskFunction_t fn;
    void *arg;
} sim_task_t;

static void *sim_task_trampoline(void *p)
{
    sim_task_t *t = (sim_task_t *)p;
    t->fn(t->arg);
    free(t);
    return NULL;
}

static BaseType_t task_spawn(TaskFunction_t fn, const char *name,
                             void *arg, unsigned priority, void **handle,
                             int core)
{
    (void)name; (void)priority; (void)core;
    sim_task_t *t = malloc(sizeof(*t));
    if (t == NULL) {
        return pdFAIL;
    }
    t->fn = fn;
    t->arg = arg;

    pthread_attr_t a;
    pthread_attr_init(&a);
    pthread_attr_setdetachstate(&a, PTHREAD_CREATE_DETACHED);
    pthread_t th;
    int rc = pthread_create(&th, &a, sim_task_trampoline, t);
    pthread_attr_destroy(&a);
    if (rc != 0) {
        free(t);
        return pdFAIL;
    }
    if (handle != NULL) {
        *handle = NULL;
    }
    return pdPASS;
}

int xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack_depth,
                void *arg, unsigned priority, void **handle)
{
    return (int)task_spawn(fn, name, arg, priority, handle, -1);
}

int xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name,
                            unsigned stack_depth, void *arg, unsigned priority,
                            void **handle, int core)
{
    return (int)task_spawn(fn, name, arg, priority, handle, core);
}

void vTaskDelay(unsigned ms) { usleep(ms * 1000); }
void vTaskDelete(void *handle) { (void)handle; pthread_exit(NULL); }

// --- سمافورها ---
SemaphoreHandle_t xSemaphoreCreateMutex(void)
{
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    pthread_mutex_init(m, NULL);
    return m;
}

SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void)
{
    pthread_mutex_t *m = malloc(sizeof(pthread_mutex_t));
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(m, &a);
    pthread_mutexattr_destroy(&a);
    return m;
}

BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, unsigned ms)
{
    (void)ms;
    pthread_mutex_lock(sem);
    return pdTRUE;
}

BaseType_t xSemaphoreGive(SemaphoreHandle_t sem)
{
    pthread_mutex_unlock(sem);
    return pdTRUE;
}

void vSemaphoreDelete(SemaphoreHandle_t sem)
{
    pthread_mutex_destroy(sem);
    free(sem);
}

// =====================================================================
// GPIO مجازی - led.c و lock.c واقعی این‌ها را صدا می‌زنند
// =====================================================================
esp_err_t gpio_config(const gpio_config_t *cfg)
{
    if (cfg != NULL && cfg->pin_bit_mask) {
        demo_log("gpio %d configured as output", cfg->pin_bit_mask);
    }
    return ESP_OK;
}

int gpio_set_level(gpio_num_t gpio, int level)
{
    if (gpio == GPIO_NUM_13) {
        demo_log("LED (GPIO13) -> %s", level ? "ON" : "OFF");
    } else if (gpio == GPIO_NUM_21) {
        demo_log("DOOR RELAY (GPIO21) -> %s", level ? "UNLOCKED" : "LOCKED");
    }
    return level;
}

esp_err_t gpio_set_direction(gpio_num_t gpio, int mode)
{
    (void)gpio; (void)mode;
    return ESP_OK;
}

void gpio_reset_pin(gpio_num_t gpio)
{
    (void)gpio;
}

// =====================================================================
// lcd_driver - قفل LVGL روی pthread بازگشتی
// =====================================================================
static pthread_mutex_t s_lvgl_mtx;
static pthread_once_t s_lvgl_once = PTHREAD_ONCE_INIT;

static void lvgl_mtx_init(void)
{
    pthread_mutexattr_t a;
    pthread_mutexattr_init(&a);
    pthread_mutexattr_settype(&a, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&s_lvgl_mtx, &a);
    pthread_mutexattr_destroy(&a);
}

void lcd_driver_lvgl_lock(void) { pthread_once(&s_lvgl_once, lvgl_mtx_init); pthread_mutex_lock(&s_lvgl_mtx); }
void lcd_driver_lvgl_unlock(void) { pthread_mutex_unlock(&s_lvgl_mtx); }

bool lcd_driver_init(void) { return true; }
lv_display_t *lcd_driver_get_display(void) { return NULL; }
void lcd_backlight_set(bool on) { (void)on; }

// =====================================================================
// WiFi مجازی - دنباله‌ی اتصال + اسکن ساختگی + اتصال از صفحه‌ی setup
// =====================================================================
static wifi_state_change_cb_t s_wifi_cb = NULL;
static volatile wifi_state_t s_wifi_state = WIFI_STATE_OFFLINE;
static char s_wifi_ssid[33] = "";
static char s_wifi_ip[20] = "";

static void wifi_set_state(wifi_state_t st)
{
    s_wifi_state = st;
    if (s_wifi_cb != NULL) {
        s_wifi_cb(st);
    }
}

void wifi_manager_init_radio(void) {}

wifi_state_t wifi_get_state(void) { return s_wifi_state; }
bool wifi_is_connected(void) { return s_wifi_state == WIFI_STATE_CONNECTED; }
void wifi_register_state_change_cb(wifi_state_change_cb_t cb) { s_wifi_cb = cb; }

static void *wifi_connect_seq(void *arg)
{
    (void)arg;
    wifi_set_state(WIFI_STATE_CONNECTING);
    usleep(2500 * 1000);
    snprintf(s_wifi_ssid, sizeof(s_wifi_ssid), "DemoNet");
    snprintf(s_wifi_ip, sizeof(s_wifi_ip), "192.168.4.1");
    wifi_set_state(WIFI_STATE_CONNECTED);
    demo_log("WiFi connected to %s (IP %s)", s_wifi_ssid, s_wifi_ip);
    return NULL;
}

static void wifi_spawn_connect(void)
{
    pthread_t th;
    pthread_create(&th, NULL, wifi_connect_seq, NULL);
    pthread_detach(th);
}

void wifi_manager_enable(void)
{
    if (!wifi_is_connected()) {
        wifi_spawn_connect();
    }
}

void wifi_manager_reconnect_from_list(void) { wifi_spawn_connect(); }

void wifi_manager_disable(void)
{
    snprintf(s_wifi_ssid, sizeof(s_wifi_ssid), "");
    snprintf(s_wifi_ip, sizeof(s_wifi_ip), "");
    wifi_set_state(WIFI_STATE_OFFLINE);
    demo_log("WiFi radio off");
}

void wifi_manager_disconnect(void) { wifi_manager_disable(); }

int wifi_manager_scan(wifi_scan_result_t *out, int max_results)
{
    // شبکه‌های ساختگی برای نمایش اسکن در صفحه‌ی setup
    static wifi_scan_result_t nets[] = {
        { "DemoNet",      -42, WIFI_AUTH_WPA2_PSK },
        { "Neighbor-2G",  -66, WIFI_AUTH_WPA2_PSK },
        { "CoffeeShop",   -74, WIFI_AUTH_OPEN },
    };
    int n = (int)(sizeof(nets) / sizeof(nets[0]));
    if (n > max_results) {
        n = max_results;
    }
    memcpy(out, nets, (size_t)n * sizeof(nets[0]));
    return n;
}

esp_err_t wifi_manager_connect_and_save(const char *ssid, const char *password)
{
    (void)password;
    usleep(2000 * 1000);   // اتصال «واقعی» مجازی - ۲ ثانیه
    snprintf(s_wifi_ssid, sizeof(s_wifi_ssid), "%s", ssid);
    snprintf(s_wifi_ip, sizeof(s_wifi_ip), "192.168.4.1");
    wifi_set_state(WIFI_STATE_CONNECTED);
    demo_log("WiFi connected to %s (IP %s)", s_wifi_ssid, s_wifi_ip);
    return ESP_OK;
}

const char *wifi_get_ip_str(void)
{
    static char buf[20] = {0};
    snprintf(buf, sizeof(buf), "%s", s_wifi_ip);
    return buf;
}

const char *wifi_get_connected_ssid(void)
{
    static char buf[33] = {0};
    snprintf(buf, sizeof(buf), "%s", s_wifi_ssid);
    return buf;
}

// =====================================================================
// MQTT مجازی - بروکری که publish ها را لاگ می‌کند و دنباله‌ی اتصال دارد
// =====================================================================
static mqtt_manager_state_change_cb_t s_mqtt_state_cb = NULL;
static volatile mqtt_manager_state_t s_mqtt_state = MQTT_MGR_STATE_UNCONFIGURED;
static char s_mqtt_host[65] = "demo-broker.local";

static void mqtt_set_state(mqtt_manager_state_t st)
{
    s_mqtt_state = st;
    if (s_mqtt_state_cb != NULL) {
        s_mqtt_state_cb(st);
    }
}

static void *mqtt_connect_seq(void *arg)
{
    (void)arg;
    usleep(2000 * 1000);
    mqtt_set_state(MQTT_MGR_STATE_CONNECTING);
    usleep(2500 * 1000);
    mqtt_set_state(MQTT_MGR_STATE_CONNECTED);
    demo_log("MQTT connected to %s (virtual broker)", s_mqtt_host);
    return NULL;
}

void mqtt_manager_init(void)
{
    pthread_t th;
    pthread_create(&th, NULL, mqtt_connect_seq, NULL);
    pthread_detach(th);
}

void mqtt_manager_register_state_change_cb(mqtt_manager_state_change_cb_t cb)
{
    s_mqtt_state_cb = cb;
}

mqtt_manager_state_t mqtt_manager_get_state(void) { return s_mqtt_state; }

bool mqtt_manager_connect_and_save(const char *host)
{
    usleep(1500 * 1000);
    snprintf(s_mqtt_host, sizeof(s_mqtt_host), "%s", host);
    demo_log("MQTT broker saved: %s", host);
    return true;
}

void mqtt_manager_enable(void) { demo_log("mqtt: enable (virtual)"); }
void mqtt_manager_disable(void) { demo_log("mqtt: disable (virtual)"); }
void mqtt_manager_prepare_for_network_loss(void) {}
void mqtt_manager_notify_network_lost(void) {}
void mqtt_manager_notify_network_available(void) {}

static void broker_log(const char *topic, const char *payload, bool retained)
{
    demo_log("[broker] %s <- %s%s", topic, payload, retained ? " (retained)" : "");
}

void mqtt_manager_publish_light_state(bool on) { broker_log("smarthome/light/state", on ? "ON" : "OFF", true); }
void mqtt_manager_publish_fan_state(bool on) { broker_log("smarthome/fan/state", on ? "ON" : "OFF", true); }
void mqtt_manager_publish_lock_state(bool unlocked) { broker_log("smarthome/lock/state", unlocked ? "UNLOCKED" : "LOCKED", true); }
void mqtt_manager_publish_presence(bool present) { broker_log("smarthome/presence/state", present ? "HOME" : "AWAY", true); }

void mqtt_manager_publish_sensor_state(float temp, float hum, float pressure)
{
    char p[96];
    snprintf(p, sizeof(p), "{\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f}",
             temp, hum, pressure);
    broker_log("smarthome/sensor/state", p, false);
}

void mqtt_manager_publish_lux(float lux)
{
    char p[32];
    snprintf(p, sizeof(p), "{\"lux\":%.1f}", lux);
    broker_log("smarthome/lux/state", p, true);
}

void mqtt_manager_publish_ml_mode(bool any_auto) { broker_log("smarthome/ml/mode", any_auto ? "AUTO" : "SHADOW", true); }

void mqtt_manager_publish_ml_stats(float p_light, float p_fan, float acc_light, float acc_fan,
                                   uint32_t n_light, uint32_t n_fan, uint32_t total_updates,
                                   bool auto_light, bool auto_fan)
{
    char p[192];
    snprintf(p, sizeof(p),
             "{\"p_light\":%.2f,\"p_fan\":%.2f,\"acc_light\":%.0f,\"acc_fan\":%.0f,"
             "\"n_light\":%u,\"n_fan\":%u,\"updates\":%u,\"auto_light\":%s,\"auto_fan\":%s}",
             p_light, p_fan, acc_light, acc_fan, (unsigned)n_light, (unsigned)n_fan,
             (unsigned)total_updates, auto_light ? "true" : "false", auto_fan ? "true" : "false");
    broker_log("smarthome/ml/stats", p, false);
}

void mqtt_manager_publish_access_event(access_event_type_t type)
{
    static const char *names[] = {
        "GRANTED_FACE", "DENIED_FACE", "GRANTED_CODE", "DENIED_CODE"
    };
    char p[48];
    snprintf(p, sizeof(p), "{\"event_type\":\"%s\"}", names[type]);
    broker_log("smarthome/access/state", p, false);
}

// =====================================================================
// NVS در حافظه - ml_model وزن‌ها را بین اجراها ذخیره می‌کند
// =====================================================================
#define NVS_MAX_ENTRIES 32

typedef struct {
    char ns[16];
    char key[16];
    uint8_t data[256];
    size_t len;
} nvs_entry_t;

static nvs_entry_t s_nvs[NVS_MAX_ENTRIES];
static int s_nvs_count = 0;
static pthread_mutex_t s_nvs_mtx = PTHREAD_MUTEX_INITIALIZER;

static nvs_entry_t *nvs_find(nvs_handle_t h, const char *key)
{
    const char *ns = (h == 0) ? "" : "?";
    // handle ایندکس+1 است و namespace همان لحظه‌ی open ذخیره شده؛ برای سادگی
    // کلیدها را در یک فضای تخت نگه می‌داریم (یک namespace در دمو کافی است)
    (void)ns;
    for (int i = 0; i < s_nvs_count; i++) {
        if (strcmp(s_nvs[i].key, key) == 0) {
            return &s_nvs[i];
        }
    }
    return NULL;
}

esp_err_t nvs_open(const char *ns, int mode, nvs_handle_t *out)
{
    (void)ns; (void)mode;
    *out = 1;
    return ESP_OK;
}

void nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }

esp_err_t nvs_set_u8(nvs_handle_t h, const char *key, uint8_t v)
{
    (void)h;
    pthread_mutex_lock(&s_nvs_mtx);
    nvs_entry_t *e = nvs_find(1, key);
    if (e == NULL && s_nvs_count < NVS_MAX_ENTRIES) {
        e = &s_nvs[s_nvs_count++];
        snprintf(e->key, sizeof(e->key), "%s", key);
    }
    if (e) {
        e->data[0] = v;
        e->len = 1;
    }
    pthread_mutex_unlock(&s_nvs_mtx);
    return ESP_OK;
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *key, uint8_t *v)
{
    (void)h;
    pthread_mutex_lock(&s_nvs_mtx);
    nvs_entry_t *e = nvs_find(1, key);
    bool ok = (e != NULL && e->len == 1);
    if (ok) {
        *v = e->data[0];
    }
    pthread_mutex_unlock(&s_nvs_mtx);
    return ok ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t nvs_set_u32(nvs_handle_t h, const char *key, uint32_t v)
{
    (void)h;
    pthread_mutex_lock(&s_nvs_mtx);
    nvs_entry_t *e = nvs_find(1, key);
    if (e == NULL && s_nvs_count < NVS_MAX_ENTRIES) {
        e = &s_nvs[s_nvs_count++];
        snprintf(e->key, sizeof(e->key), "%s", key);
    }
    if (e) {
        memcpy(e->data, &v, 4);
        e->len = 4;
    }
    pthread_mutex_unlock(&s_nvs_mtx);
    return ESP_OK;
}

esp_err_t nvs_get_u32(nvs_handle_t h, const char *key, uint32_t *v)
{
    (void)h;
    pthread_mutex_lock(&s_nvs_mtx);
    nvs_entry_t *e = nvs_find(1, key);
    bool ok = (e != NULL && e->len == 4);
    if (ok) {
        memcpy(v, e->data, 4);
    }
    pthread_mutex_unlock(&s_nvs_mtx);
    return ok ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t nvs_set_blob(nvs_handle_t h, const char *key, const void *data, size_t len)
{
    (void)h;
    if (len > sizeof(s_nvs[0].data)) {
        return ESP_ERR_INVALID_SIZE;
    }
    pthread_mutex_lock(&s_nvs_mtx);
    nvs_entry_t *e = nvs_find(1, key);
    if (e == NULL && s_nvs_count < NVS_MAX_ENTRIES) {
        e = &s_nvs[s_nvs_count++];
        snprintf(e->key, sizeof(e->key), "%s", key);
    }
    if (e) {
        memcpy(e->data, data, len);
        e->len = len;
    }
    pthread_mutex_unlock(&s_nvs_mtx);
    return ESP_OK;
}

esp_err_t nvs_get_blob(nvs_handle_t h, const char *key, void *out, size_t *len)
{
    (void)h;
    pthread_mutex_lock(&s_nvs_mtx);
    nvs_entry_t *e = nvs_find(1, key);
    bool ok = (e != NULL);
    if (ok) {
        size_t cpy = e->len < *len ? e->len : *len;
        memcpy(out, e->data, cpy);
        *len = e->len;
    }
    pthread_mutex_unlock(&s_nvs_mtx);
    return ok ? ESP_OK : ESP_ERR_NOT_FOUND;
}

esp_err_t nvs_erase_all(nvs_handle_t h)
{
    (void)h;
    pthread_mutex_lock(&s_nvs_mtx);
    s_nvs_count = 0;
    pthread_mutex_unlock(&s_nvs_mtx);
    return ESP_OK;
}

// =====================================================================
// رمزهای دمو - قفل 1234 / تنظیمات 0000
// =====================================================================
static char s_pw[2][PASSWORD_MAX_LEN + 1] = { "1234", "0000" };

esp_err_t password_manager_init(void) { return ESP_OK; }

bool password_manager_verify(password_kind_t kind, const char *input)
{
    const char *ref = (kind == PASSWORD_KIND_SETTINGS) ? s_pw[1] : s_pw[0];
    return strcmp(input, ref) == 0;
}

esp_err_t password_manager_set(password_kind_t kind, const char *new_password)
{
    snprintf(s_pw[(kind == PASSWORD_KIND_SETTINGS) ? 1 : 0],
             sizeof(s_pw[0]), "%s", new_password);
    return ESP_OK;
}

// =====================================================================
// چهره‌ی مجازی - دیتابیس دمویی + ثبت/حذف/تشخیص اسکریپتی
// =====================================================================
static face_db_person_t s_persons[FACE_DB_MAX_ENTRIES];
static size_t s_person_count = 2;

esp_err_t face_db_init(void)
{
    snprintf(s_persons[0].name, sizeof(s_persons[0].name), "Soroush");
    s_persons[0].sample_count = 3;
    snprintf(s_persons[1].name, sizeof(s_persons[1].name), "Sara");
    s_persons[1].sample_count = 2;
    return ESP_OK;
}

size_t face_db_get_persons(face_db_person_t *out, size_t max)
{
    size_t n = s_person_count < max ? s_person_count : max;
    memcpy(out, s_persons, n * sizeof(s_persons[0]));
    return n;
}

esp_err_t face_db_remove_by_name(const char *name, uint16_t *removed_ids,
                                 size_t max_ids, size_t *removed_count)
{
    (void)removed_ids; (void)max_ids;
    *removed_count = 0;
    for (size_t i = 0; i < s_person_count; i++) {
        if (strcmp(s_persons[i].name, name) == 0) {
            demo_log("face_db: removed all samples of \"%s\"", name);
            s_persons[i] = s_persons[s_person_count - 1];
            s_person_count--;
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t face_db_add(uint16_t id, const char *name)
{
    (void)id;
    if (s_person_count >= FACE_DB_MAX_ENTRIES) {
        return ESP_ERR_NO_MEM;
    }
    snprintf(s_persons[s_person_count].name, sizeof(s_persons[0].name), "%s", name);
    s_persons[s_person_count].sample_count = 1;
    s_person_count++;
    return ESP_OK;
}

esp_err_t face_db_remove(uint16_t id) { (void)id; return ESP_OK; }
size_t face_db_get_list(face_db_entry_t *out, size_t max) { (void)out; (void)max; return 0; }
bool face_db_get_name(uint16_t id, char *out, size_t sz) { (void)id; (void)out; (void)sz; return false; }
size_t face_db_count_by_name(const char *name)
{
    for (size_t i = 0; i < s_person_count; i++) {
        if (strcmp(s_persons[i].name, name) == 0) {
            return s_persons[i].sample_count;
        }
    }
    return 0;
}

esp_err_t face_recognition_init(void) { return ESP_OK; }
esp_err_t face_recognition_delete(uint16_t id) { (void)id; return ESP_OK; }
esp_err_t face_recognition_process(const uint8_t *jpeg, size_t len, bool *unlocked, int *id, float *sim)
{
    (void)jpeg; (void)len;
    *unlocked = false;
    *id = -1;
    *sim = 0.0f;
    return ESP_ERR_NOT_FOUND;
}
esp_err_t face_recognition_enroll(const uint8_t *jpeg, size_t len, int *new_id)
{
    (void)jpeg; (void)len;
    *new_id = 100;
    return ESP_OK;
}

// =====================================================================
// توکن ثبت چهره مجازی
// =====================================================================
esp_err_t enroll_token_init(void) { return ESP_OK; }

void enroll_token_generate(enroll_token_purpose_t purpose, char *out, size_t sz)
{
    (void)purpose;
    snprintf(out, sz, "%06u", (unsigned)(esp_random() % 1000000));
}

bool enroll_token_validate(enroll_token_purpose_t purpose, const char *token)
{
    (void)purpose; (void)token;
    return false;
}

void enroll_token_invalidate(enroll_token_purpose_t purpose) { (void)purpose; }

// =====================================================================
// حضور و غیاب مجازی - غیرفعال ولی با کانفیگ دمویی تا QR صفحه رندر شود
// =====================================================================
esp_err_t attendance_init(void) { return ESP_OK; }
bool attendance_is_enabled(void) { return true; }
bool attendance_get_config(char *url, size_t url_sz, char *secret, size_t secret_sz, bool *enabled)
{
    snprintf(url, url_sz, "http://demo-server.local:8080");
    snprintf(secret, secret_sz, "demo-secret");
    *enabled = true;
    return true;
}
esp_err_t attendance_set_config(const char *url, const char *secret, bool enabled)
{
    (void)url; (void)secret; (void)enabled;
    return ESP_OK;
}
int attendance_queue_count(void) { return 0; }
esp_err_t attendance_record(const char *name, uint16_t id, attendance_event_t ev,
                            float sim, bool *dup)
{
    (void)name; (void)id; (void)ev; (void)sim;
    if (dup) *dup = false;
    return ESP_OK;
}
esp_err_t attendance_report_event(attendance_event_t ev, const char *name,
                                  uint16_t id, float sim)
{
    (void)ev; (void)name; (void)id; (void)sim;
    return ESP_OK;
}
esp_err_t attendance_test_server(void) { return ESP_OK; }

// =====================================================================
// time_sync مجازی - ۲ ثانیه بعد از بوت ساعت «sync» می‌شود
// =====================================================================
static int64_t s_tsync_ready_at = 0;

void time_sync_init(void)
{
    s_tsync_ready_at = esp_timer_get_time() + 2000000;
    demo_log("time sync: waiting for NTP (virtual)...");
}

bool time_sync_is_ready(void)
{
    return s_tsync_ready_at != 0 && esp_timer_get_time() >= s_tsync_ready_at;
}

// =====================================================================
// wifi_config مجازی - یک شبکه‌ی شناخته‌شده
// =====================================================================
esp_err_t wifi_config_init(void) { return ESP_OK; }
int wifi_config_get_all(wifi_known_network_t *out, int max_count)
{
    if (max_count <= 0) {
        return 0;
    }
    snprintf(out[0].ssid, sizeof(out[0].ssid), "DemoNet");
    snprintf(out[0].password, sizeof(out[0].password), "demo1234");
    return 1;
}
esp_err_t wifi_config_promote(const char *ssid, const char *password)
{
    (void)ssid; (void)password;
    return ESP_OK;
}
esp_err_t wifi_config_clear_all(void) { return ESP_OK; }
esp_err_t wifi_config_remove(const char *ssid) { (void)ssid; return ESP_ERR_NOT_FOUND; }

// =====================================================================
// mqtt_config مجازی
// =====================================================================
esp_err_t mqtt_config_init(void) { return ESP_OK; }
bool mqtt_config_get_host(char *out, size_t sz)
{
    snprintf(out, sz, "%s", s_mqtt_host);
    return s_mqtt_host[0] != '\0';
}
esp_err_t mqtt_config_set_host(const char *host) { (void)host; return ESP_OK; }
