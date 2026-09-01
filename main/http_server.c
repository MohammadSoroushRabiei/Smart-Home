#include "http_server.h"
#include "face_recognition.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>

static const char *TAG = "HTTP";

#define FACE_IMAGE_MAX_SIZE     (300 * 1024)
#define FACE_WORKER_STACK_SIZE  16384   // AI decode + inference نیاز به stack نسبتاً بزرگی دارد
#define FACE_WORKER_PRIORITY    3

typedef enum {
    FACE_OP_RECOGNIZE,
    FACE_OP_ENROLL,
} face_op_t;

typedef struct {
    httpd_req_t *req;
    face_op_t op;
} face_work_item_t;

static QueueHandle_t s_face_queue = NULL;

// ---------------------------------------------------------------------
// هندلرهای ساده (بدون تغییر منطقی)
// ---------------------------------------------------------------------

static esp_err_t led_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /led received");
    const char *response = led_is_on() ? "LED: ON" : "LED: OFF";
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_toggle_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /led/toggle received");
    led_toggle(LED);
    const char *response = led_is_on() ? "LED: ON" : "LED: OFF";
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html><html><head><title>Smart Home</title></head><body>"
        "<h1>Smart Home</h1><h2 id=\"led-status\">LED: </h2>"
        "<button id=\"led-toggle\">Toggle LED</button>"
        "<script>"
        "fetch(\"/led\").then(response => response.text()).then(data => {document.getElementById(\"led-status\").textContent = data;});"
        "const btn = document.getElementById(\"led-toggle\");"
        "btn.addEventListener('click', function() {"
        "fetch(\"/led/toggle\").then(response => response.text()).then(data => {document.getElementById(\"led-status\").textContent = data;});"
        "});"
        "</script></body></html>";
    httpd_resp_set_type(req, "text/html");
    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

// ---------------------------------------------------------------------
// پردازش سنگین چهره - این تابع فقط داخل face_worker_task اجرا می‌شود
// ---------------------------------------------------------------------

static esp_err_t receive_jpeg_body(httpd_req_t *req, uint8_t **out_buf, size_t *out_len)
{
    size_t content_len = req->content_len;
    if (content_len == 0 || content_len > FACE_IMAGE_MAX_SIZE) {
        return ESP_ERR_INVALID_SIZE;
    }

    uint8_t *buf = (uint8_t *)heap_caps_malloc(content_len, MALLOC_CAP_SPIRAM);
    if (!buf) {
        return ESP_ERR_NO_MEM;
    }

    int total_received = 0;
    while (total_received < content_len) {
        int received = httpd_req_recv(req, (char *)buf + total_received, content_len - total_received);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            heap_caps_free(buf);
            return ESP_FAIL;
        }
        total_received += received;
    }

    *out_buf = buf;
    *out_len = total_received;
    return ESP_OK;
}

static void handle_recognize(httpd_req_t *req)
{
    uint8_t *jpeg_buf = NULL;
    size_t len = 0;

    esp_err_t ret = receive_jpeg_body(req, &jpeg_buf, &len);
    if (ret == ESP_ERR_INVALID_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid image size");
        return;
    } else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive image");
        return;
    }

    bool is_unlocked = false;
    int detected_id = -1;
    ret = face_recognition_process(jpeg_buf, len, &is_unlocked, &detected_id);
    heap_caps_free(jpeg_buf);

    if (ret == ESP_ERR_NOT_FOUND) {
        httpd_resp_set_status(req, "404 No Face");
        httpd_resp_sendstr(req, "No face detected");
        return;
    } else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Processing failed");
        return;
    }

    if (is_unlocked) {
        char resp[64];
        snprintf(resp, sizeof(resp), "Access Granted! Welcome User ID: %d", detected_id);
        httpd_resp_sendstr(req, resp);
        // نکته: اتصال به app_state_set_lock(true) در اینجا اضافه خواهد شد (بخش ۳)
    } else {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Access Denied: Unknown Face");
    }
}

static void handle_enroll(httpd_req_t *req)
{
    uint8_t *jpeg_buf = NULL;
    size_t len = 0;

    esp_err_t ret = receive_jpeg_body(req, &jpeg_buf, &len);
    if (ret == ESP_ERR_INVALID_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid image size");
        return;
    } else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive image");
        return;
    }

    int new_id = 0;
    ret = face_recognition_enroll(jpeg_buf, len, &new_id);
    heap_caps_free(jpeg_buf);

    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Enrollment failed");
        return;
    }

    char resp[64];
    snprintf(resp, sizeof(resp), "Face enrolled successfully with ID: %d", new_id);
    httpd_resp_sendstr(req, resp);
}

// ---------------------------------------------------------------------
// Task اختصاصی: تنها مصرف‌کننده‌ی صف کار
// ---------------------------------------------------------------------

static void face_worker_task(void *arg)
{
    face_work_item_t item;

    while (1) {
        if (xQueueReceive(s_face_queue, &item, portMAX_DELAY) == pdTRUE) {
            ESP_LOGI(TAG, "Face worker processing %s request",
                     item.op == FACE_OP_RECOGNIZE ? "recognize" : "enroll");

            if (item.op == FACE_OP_RECOGNIZE) {
                handle_recognize(item.req);
            } else {
                handle_enroll(item.req);
            }

            httpd_req_async_handler_complete(item.req);
        }
    }
}

// ---------------------------------------------------------------------
// هندلرهای async (سبک، فقط صف را پر می‌کنند و فوراً برمی‌گردند)
// ---------------------------------------------------------------------

static esp_err_t enqueue_face_request(httpd_req_t *req, face_op_t op)
{
    httpd_req_t *copy = NULL;
    esp_err_t err = httpd_req_async_handler_begin(req, &copy);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start async request");
        return ESP_FAIL;
    }

    face_work_item_t item = { .req = copy, .op = op };

    if (xQueueSend(s_face_queue, &item, 0) != pdTRUE) {
        // صف پره یعنی یک درخواست دیگه در حال پردازشه
        httpd_resp_set_status(copy, "503 Server Busy");
        httpd_resp_sendstr(copy, "Face recognition is busy processing another request, try again shortly");
        httpd_req_async_handler_complete(copy);
        return ESP_OK;
    }

    return ESP_OK;   // handler HTTP فوراً برمی‌گرده؛ تسک اصلی همچنان آزاده
}

static esp_err_t face_recognize_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/face/recognize received (enqueueing)");
    return enqueue_face_request(req, FACE_OP_RECOGNIZE);
}

static esp_err_t face_enroll_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/face/enroll received (enqueueing)");
    return enqueue_face_request(req, FACE_OP_ENROLL);
}

// ---------------------------------------------------------------------
// ثبت URI ها و راه‌اندازی سرور
// ---------------------------------------------------------------------

static const httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL };
static const httpd_uri_t led_uri = { .uri = "/led", .method = HTTP_GET, .handler = led_handler, .user_ctx = NULL };
static const httpd_uri_t led_toggle_uri = { .uri = "/led/toggle", .method = HTTP_GET, .handler = led_toggle_handler, .user_ctx = NULL };
static const httpd_uri_t face_recognize_uri = { .uri = "/api/face/recognize", .method = HTTP_POST, .handler = face_recognize_handler, .user_ctx = NULL };
static const httpd_uri_t face_enroll_uri = { .uri = "/api/face/enroll", .method = HTTP_POST, .handler = face_enroll_handler, .user_ctx = NULL };

httpd_handle_t http_server_start(void)
{
    s_face_queue = xQueueCreate(1, sizeof(face_work_item_t));
    if (s_face_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create face worker queue");
        return NULL;
    }

    xTaskCreate(face_worker_task, "face_worker", FACE_WORKER_STACK_SIZE, NULL,
                FACE_WORKER_PRIORITY, NULL);

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.stack_size = 8192;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return NULL;
    }

    httpd_register_uri_handler(server, &root_uri);
    httpd_register_uri_handler(server, &led_uri);
    httpd_register_uri_handler(server, &led_toggle_uri);
    httpd_register_uri_handler(server, &face_recognize_uri);
    httpd_register_uri_handler(server, &face_enroll_uri);

    ESP_LOGI(TAG, "HTTP server started");
    return server;
}