#include "attendance.h"
#include "time_sync.h"
#include "wifi_manager.h"

#include <string.h>
#include <stdio.h>
#include <time.h>
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "attendance";

// ---------------------------------------------------------------------
// ثابت‌ها
// ---------------------------------------------------------------------

#define NVS_NAMESPACE       "attendance"
#define NVS_KEY_URL         "server_url"
#define NVS_KEY_SECRET      "secret"
#define NVS_KEY_ENABLED     "enabled"
#define NVS_KEY_Q_COUNT     "q_count"
#define NVS_KEY_Q_TAIL      "q_tail"
#define NVS_KEY_SLOT_FMT    "q%d"

// ظرفیت صف معوق در NVS - با پر شدن، قدیمی‌ترین رکورد قربانی می‌شود
#define QUEUE_SLOTS         32
#define POST_TIMEOUT_MS     10000
#define DEDUP_WINDOW_MS     (60 * 1000)
#define FLUSH_PERIOD_MS     (60 * 1000)     // تلاش دوره‌ای حتی بدون رکورد جدید
#define DELAY_BETWEEN_POSTS_MS  300
#define ATT_TASK_STACK_SIZE 8192
#define ATT_TASK_PRIORITY   2
#define PAYLOAD_MAX         320

// ---------------------------------------------------------------------
// رکورد - عمداً فشرده؛ هر اسلات یک blob کوچک در NVS است
// ---------------------------------------------------------------------

typedef struct __attribute__((packed)) {
    uint32_t ts;                    // epoch (ثانیه) - لحظه‌ی ثبت، نه ارسال
    char     name[ATTENDANCE_NAME_MAX];
    uint16_t person_id;
    uint8_t  event;                 // attendance_event_t
    uint8_t  similarity_pct;        // 0..100
} attendance_record_t;

// ---------------------------------------------------------------------
// وضعیت ماژول
// ---------------------------------------------------------------------

static SemaphoreHandle_t s_mutex;        // محافظ کانفیگ + صف + ضدانتشار
static SemaphoreHandle_t s_flush_sem;    // بیدار کردن تسک ارسال

static char s_url[ATTENDANCE_SERVER_URL_MAX_LEN];   // آدرس پایه (بدون مسیر)
static char s_secret[ATTENDANCE_SECRET_MAX_LEN + 1];
static bool s_enabled;

// صف حلقوی روی NVS: blob های q0..q31 + دو شمارنده‌ی پایدار.
// head از tail و count ساخته می‌شود؛ فقط tail/count ذخیره می‌شوند.
static uint8_t s_q_tail;
static uint8_t s_q_count;

// ضدانتشار سبک (فقط RAM - با ریبوت پاک می‌شود، قابل قبول)
static uint16_t s_last_id;
static uint8_t  s_last_event;
static int64_t  s_last_ms;

// ---------------------------------------------------------------------
// ابزار
// ---------------------------------------------------------------------

// کپی رشته با escape کاراکترهای خاص JSON - هم‌معنای نسخه‌ی http_server.c
static void json_escape(const char *in, char *out, size_t out_size)
{
    size_t oi = 0;
    for (const char *p = in; *p != '\0' && oi + 1 < out_size; p++) {
        unsigned char c = (unsigned char)*p;
        if (c == '"' || c == '\\') {
            if (oi + 2 >= out_size) {
                break;
            }
            out[oi++] = '\\';
        }
        out[oi++] = (char)c;
    }
    out[oi] = '\0';
}

static const char *event_str(uint8_t event)
{
    switch ((attendance_event_t)event) {
    case ATT_EVENT_ATTENDANCE_IN:  return "attendance_in";
    case ATT_EVENT_ATTENDANCE_OUT: return "attendance_out";
    case ATT_EVENT_DOOR_FACE:      return "door_face";
    case ATT_EVENT_DOOR_CODE:      return "door_code";
    case ATT_EVENT_TEST:           return "test";
    default:                       return "unknown";
    }
}

static bool is_attendance_event(uint8_t event)
{
    return event == ATT_EVENT_ATTENDANCE_IN || event == ATT_EVENT_ATTENDANCE_OUT;
}

static void slot_key(uint8_t slot, char *out, size_t out_size)
{
    snprintf(out, out_size, NVS_KEY_SLOT_FMT, (int)slot);
}

static void copy_str(char *dst, size_t dst_size, const char *src)
{
    strncpy(dst, src, dst_size - 1);
    dst[dst_size - 1] = '\0';
}

// ---------------------------------------------------------------------
// صف NVS - توابع این بخش فقط با نگه‌داشتن s_mutex صدا زده می‌شوند
// ---------------------------------------------------------------------

static esp_err_t queue_persist_meta(nvs_handle_t handle)
{
    esp_err_t ret = nvs_set_u8(handle, NVS_KEY_Q_COUNT, s_q_count);
    if (ret == ESP_OK) {
        ret = nvs_set_u8(handle, NVS_KEY_Q_TAIL, s_q_tail);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle);
    }
    return ret;
}

static esp_err_t queue_append_locked(const attendance_record_t *rec)
{
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    if (s_q_count == QUEUE_SLOTS) {
        char key[8];
        slot_key(s_q_tail, key, sizeof(key));
        nvs_erase_key(handle, key);   // «وجود نداشتن» مهم نیست
        s_q_tail = (uint8_t)((s_q_tail + 1) % QUEUE_SLOTS);
        s_q_count--;
        ESP_LOGW(TAG, "Queue full - oldest record dropped");
    }

    uint8_t head = (uint8_t)((s_q_tail + s_q_count) % QUEUE_SLOTS);
    char key[8];
    slot_key(head, key, sizeof(key));

    ret = nvs_set_blob(handle, key, rec, sizeof(*rec));
    if (ret == ESP_OK) {
        s_q_count++;
        ret = queue_persist_meta(handle);
    }
    nvs_close(handle);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to append record to NVS: %s", esp_err_to_name(ret));
    }
    return ret;
}

// خواندن قدیمی‌ترین رکورد بدون حذف آن
static esp_err_t queue_peek_locked(attendance_record_t *rec)
{
    if (s_q_count == 0) {
        return ESP_ERR_NOT_FOUND;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret != ESP_OK) {
        return ret;
    }

    char key[8];
    slot_key(s_q_tail, key, sizeof(key));
    size_t len = sizeof(*rec);
    ret = nvs_get_blob(handle, key, rec, &len);
    nvs_close(handle);

    if (ret != ESP_OK || len != sizeof(*rec)) {
        ESP_LOGE(TAG, "Queued record %s unreadable", key);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// حذف قدیمی‌ترین رکورد، اما فقط اگر همان رکوردی باشد که ارسال کرده‌ایم.
// بین peek و advance ممکن است رکورد جدیدی آمده و صف پر بوده باشد و
// drop-oldest همین رکورد را حذف کرده باشد؛ در آن صورت نباید رکوردِ
// تازه‌ترِ بعدی را به اشتباه حذف کنیم.
static void queue_advance_locked(const attendance_record_t *expected)
{
    attendance_record_t current;
    if (queue_peek_locked(&current) != ESP_OK ||
        memcmp(&current, expected, sizeof(current)) != 0) {
        return;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return;
    }

    char key[8];
    slot_key(s_q_tail, key, sizeof(key));
    nvs_erase_key(handle, key);
    s_q_tail = (uint8_t)((s_q_tail + 1) % QUEUE_SLOTS);
    s_q_count--;

    if (queue_persist_meta(handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist queue meta after advance");
    }
    nvs_close(handle);
}

// حذف بی‌قید و شرط قدیمی‌ترین اسلات - برای رکورد خرابی که خواندنش شکست
// خورده (advance شرطی در این حالت هیچ‌وقت پاک نمی‌کند و تسک حلقه می‌زند)
static void queue_drop_head_locked(void)
{
    if (s_q_count == 0) {
        return;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        return;
    }

    char key[8];
    slot_key(s_q_tail, key, sizeof(key));
    nvs_erase_key(handle, key);
    s_q_tail = (uint8_t)((s_q_tail + 1) % QUEUE_SLOTS);
    s_q_count--;

    if (queue_persist_meta(handle) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to persist queue meta after drop");
    }
    nvs_close(handle);
}

// ---------------------------------------------------------------------
// ارسال به سرور لوکال
// ---------------------------------------------------------------------

static void build_payload(const attendance_record_t *rec, const char *secret,
                          char *payload, size_t payload_size)
{
    char ts_str[24] = "unknown";
    time_t t = (time_t)rec->ts;
    struct tm tm_info;
    if (rec->ts > 0 && localtime_r(&t, &tm_info) != NULL) {
        strftime(ts_str, sizeof(ts_str), "%Y-%m-%d %H:%M:%S", &tm_info);
    }

    char name_esc[2 * ATTENDANCE_NAME_MAX + 2];
    json_escape(rec->name, name_esc, sizeof(name_esc));

    snprintf(payload, payload_size,
             "{\"secret\":\"%s\",\"event\":\"%s\",\"ts\":\"%s\",\"name\":\"%s\","
             "\"id\":%u,\"similarity\":%u}",
             secret, event_str(rec->event), ts_str, name_esc,
             (unsigned)rec->person_id, (unsigned)rec->similarity_pct);
}

// POST کردن payload به {base}/api/event. معیار موفقیت: پاسخ 2xx.
static esp_err_t post_event(const char *base_url, const char *payload)
{
    char url[ATTENDANCE_SERVER_URL_MAX_LEN + 16];
    snprintf(url, sizeof(url), "%s/api/event", base_url);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = POST_TIMEOUT_MS,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,   // برای http:// نادیده گرفته می‌شود
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "esp_http_client_init failed");
        return ESP_FAIL;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    // بدون این، perform یک POST با Content-Length: صفر می‌فرستد و سرور
    // 422 برمی‌گرداند (باگ واقعی که در اولین تست برد لو رفت)
    esp_http_client_set_post_field(client, payload, strlen(payload));
    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Server POST failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    if (status < 200 || status >= 300) {
        ESP_LOGE(TAG, "Server POST got HTTP %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}

// ---------------------------------------------------------------------
// تسک ارسال
// ---------------------------------------------------------------------

// بیدار می‌شود با: رکورد جدید، تایمر دوره‌ای (۶۰ ثانیه) و در ابتدای بوت -
// تا صف معوقِ مانده از قبلِ ریبوت هم بالاخره ارسال شود.
static void attendance_task(void *arg)
{
    while (1) {
        xSemaphoreTake(s_flush_sem, pdMS_TO_TICKS(FLUSH_PERIOD_MS));

        if (!wifi_is_connected()) {
            continue;   // بدون WiFi رکوردها در NVS می‌مانند
        }

        while (1) {
            attendance_record_t rec;
            char url[ATTENDANCE_SERVER_URL_MAX_LEN];
            char secret[ATTENDANCE_SECRET_MAX_LEN + 1];
            char payload[PAYLOAD_MAX];

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            esp_err_t peek = queue_peek_locked(&rec);
            if (peek == ESP_OK) {
                copy_str(url, sizeof(url), s_url);
                copy_str(secret, sizeof(secret), s_secret);
            }
            xSemaphoreGive(s_mutex);

            if (peek == ESP_ERR_NOT_FOUND) {
                break;   // صف خالی - منتظر رکورد بعدی
            }
            if (peek != ESP_OK) {
                // رکورد خراب - اسلاتش را بی‌قید و شرط آزاد کن تا صف گیر نکند
                xSemaphoreTake(s_mutex, portMAX_DELAY);
                queue_drop_head_locked();
                xSemaphoreGive(s_mutex);
                continue;
            }

            build_payload(&rec, secret, payload, sizeof(payload));
            if (post_event(url, payload) != ESP_OK) {
                break;   // الان نمی‌شود فرستاد - دفعه‌ی بعد
            }

            xSemaphoreTake(s_mutex, portMAX_DELAY);
            queue_advance_locked(&rec);
            xSemaphoreGive(s_mutex);

            ESP_LOGI(TAG, "Sent: %s (%s) ts=%u - %d left in queue",
                     rec.name, event_str(rec.event),
                     (unsigned)rec.ts, attendance_queue_count());
            vTaskDelay(pdMS_TO_TICKS(DELAY_BETWEEN_POSTS_MS));
        }
    }
}

// ---------------------------------------------------------------------
// API عمومی
// ---------------------------------------------------------------------

esp_err_t attendance_init(void)
{
    s_mutex = xSemaphoreCreateMutex();
    s_flush_sem = xSemaphoreCreateBinary();
    if (s_mutex == NULL || s_flush_sem == NULL) {
        return ESP_ERR_NO_MEM;
    }

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (ret == ESP_OK) {
        size_t len = sizeof(s_url);
        nvs_get_str(handle, NVS_KEY_URL, s_url, &len);
        len = sizeof(s_secret);
        nvs_get_str(handle, NVS_KEY_SECRET, s_secret, &len);
        uint8_t en = 0;
        nvs_get_u8(handle, NVS_KEY_ENABLED, &en);
        s_enabled = (en == 1);
        nvs_get_u8(handle, NVS_KEY_Q_COUNT, &s_q_count);
        nvs_get_u8(handle, NVS_KEY_Q_TAIL, &s_q_tail);
        nvs_close(handle);
    }

    // سالم‌سازی شمارنده‌های صف در برابر داده‌ی خراب
    if (s_q_tail >= QUEUE_SLOTS) {
        s_q_tail = 0;
    }
    if (s_q_count > QUEUE_SLOTS) {
        s_q_count = QUEUE_SLOTS;
    }
    if (s_q_count > 0) {
        ESP_LOGI(TAG, "%u queued record(s) restored from NVS", (unsigned)s_q_count);
    }

    if (xTaskCreate(attendance_task, "attendance", ATT_TASK_STACK_SIZE,
                    NULL, ATT_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create attendance task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Attendance module initialized (enabled=%d, server=%s)",
             s_enabled, s_url);
    return ESP_OK;
}

bool attendance_is_enabled(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool enabled = s_enabled && s_url[0] != '\0';
    xSemaphoreGive(s_mutex);
    return enabled;
}

bool attendance_get_config(char *url, size_t url_size,
                           char *secret, size_t secret_size,
                           bool *enabled)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (url != NULL && url_size > 0) {
        copy_str(url, url_size, s_url);
    }
    if (secret != NULL && secret_size > 0) {
        copy_str(secret, secret_size, s_secret);
    }
    if (enabled != NULL) {
        *enabled = s_enabled;
    }
    bool configured = (s_url[0] != '\0');
    xSemaphoreGive(s_mutex);
    return configured;
}

esp_err_t attendance_set_config(const char *url, const char *secret, bool enabled)
{
    if (url == NULL) {
        url = "";
    }
    // کاری با ورودی کاربر نمی‌کنیم؛ فقط یک اسلش انتهایی را تحمل می‌کنیم
    char clean[ATTENDANCE_SERVER_URL_MAX_LEN];
    copy_str(clean, sizeof(clean), url);
    size_t url_len = strlen(clean);
    while (url_len > 0 && clean[url_len - 1] == '/') {
        clean[--url_len] = '\0';
    }
    if (url_len >= ATTENDANCE_SERVER_URL_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }
    // در حالت فعال، آدرسِ پایه با پروتکل صریح می‌خواهیم
    if (enabled) {
        bool http = (url_len >= 7 && strncmp(clean, "http://", 7) == 0);
        bool https = (url_len >= 8 && strncmp(clean, "https://", 8) == 0);
        if (url_len == 0 || (!http && !https)) {
            return ESP_ERR_INVALID_ARG;
        }
    }

    bool have_secret = (secret != NULL && secret[0] != '\0');
    if (have_secret && strlen(secret) > ATTENDANCE_SECRET_MAX_LEN) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    nvs_handle_t handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (ret == ESP_OK) {
        ret = nvs_set_str(handle, NVS_KEY_URL, clean);
        if (ret == ESP_OK && have_secret) {
            ret = nvs_set_str(handle, NVS_KEY_SECRET, secret);
        }
        if (ret == ESP_OK) {
            ret = nvs_set_u8(handle, NVS_KEY_ENABLED, enabled ? 1 : 0);
        }
        if (ret == ESP_OK) {
            ret = nvs_commit(handle);
        }
        nvs_close(handle);
    }

    if (ret == ESP_OK) {
        copy_str(s_url, sizeof(s_url), clean);
        if (have_secret) {
            copy_str(s_secret, sizeof(s_secret), secret);
        }
        s_enabled = enabled;
        ESP_LOGI(TAG, "Config saved (enabled=%d, url=%s%s)",
                 enabled, clean, have_secret ? ", secret updated" : "");
    } else {
        ESP_LOGE(TAG, "Failed to save config: %s", esp_err_to_name(ret));
    }

    xSemaphoreGive(s_mutex);
    return ret;
}

int attendance_queue_count(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int count = s_q_count;
    xSemaphoreGive(s_mutex);
    return count;
}

esp_err_t attendance_record(const char *name, uint16_t person_id,
                            attendance_event_t event, float similarity,
                            bool *was_duplicate)
{
    if (was_duplicate != NULL) {
        *was_duplicate = false;
    }
    if (!attendance_is_enabled()) {
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!is_attendance_event((uint8_t)event)) {
        return ESP_ERR_INVALID_ARG;
    }
    if (!time_sync_is_ready()) {
        return ESP_ERR_INVALID_STATE;
    }

    return attendance_report_event(event, name, person_id, similarity);
}

esp_err_t attendance_report_event(attendance_event_t event, const char *name,
                                  uint16_t person_id, float similarity)
{
    if (event != ATT_EVENT_TEST && !attendance_is_enabled()) {
        return ESP_ERR_NOT_SUPPORTED;
    }

    int64_t now_ms = esp_timer_get_time() / 1000;
    bool dup = false;
    esp_err_t ret = ESP_OK;

    xSemaphoreTake(s_mutex, portMAX_DELAY);

    // ضدانتشار فقط برای ورود/خروج - درب ممکن است پشت‌سرهم باز شود
    dup = (is_attendance_event((uint8_t)event) &&
           person_id == s_last_id && (uint8_t)event == s_last_event &&
           now_ms - s_last_ms < DEDUP_WINDOW_MS &&
           now_ms >= s_last_ms);

    if (!dup) {
        attendance_record_t rec = {0};
        rec.ts = time_sync_is_ready() ? (uint32_t)time(NULL) : 0;
        copy_str(rec.name, sizeof(rec.name), name ? name : "");
        rec.person_id = person_id;
        rec.event = (uint8_t)event;
        float pct = similarity * 100.0f;
        if (pct < 0.0f) {
            pct = 0.0f;
        }
        if (pct > 100.0f) {
            pct = 100.0f;
        }
        rec.similarity_pct = (uint8_t)(pct + 0.5f);

        ret = queue_append_locked(&rec);
        if (ret == ESP_OK) {
            s_last_id = person_id;
            s_last_event = (uint8_t)event;
            s_last_ms = now_ms;
            ESP_LOGI(TAG, "Queued %s (%s) id=%u sim=%u%%",
                     rec.name, event_str(rec.event), person_id,
                     (unsigned)rec.similarity_pct);
        }
    } else {
        ESP_LOGI(TAG, "Duplicate %s suppressed (id=%u)", event_str((uint8_t)event), person_id);
    }

    xSemaphoreGive(s_mutex);

    if (ret == ESP_OK && !dup) {
        xSemaphoreGive(s_flush_sem);   // بیدار کردن تسک ارسال
    }
    return ret;
}

esp_err_t attendance_test_server(void)
{
    char base[ATTENDANCE_SERVER_URL_MAX_LEN];
    char secret[ATTENDANCE_SECRET_MAX_LEN + 1];
    bool enabled = false;
    bool configured;

    configured = attendance_get_config(base, sizeof(base), secret, sizeof(secret), &enabled);
    if (!configured || !enabled) {
        return ESP_ERR_INVALID_STATE;
    }

    // بدترین حالت: URL کامل + /health?secret= + secret کامل
    char url[ATTENDANCE_SERVER_URL_MAX_LEN + ATTENDANCE_SECRET_MAX_LEN + 24];
    snprintf(url, sizeof(url), "%s/health?secret=%s", base, secret);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = POST_TIMEOUT_MS,
        .buffer_size = 1024,
        .buffer_size_tx = 1024,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Server health check failed: %s", esp_err_to_name(err));
        return ESP_FAIL;
    }
    if (status != 200) {
        ESP_LOGE(TAG, "Server health check got HTTP %d", status);
        return ESP_FAIL;
    }
    return ESP_OK;
}
