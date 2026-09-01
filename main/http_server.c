#include "http_server.h"
#include "face_recognition.h"
#include "esp_log.h"
#include "led.h"
#include <string.h>

static const char *TAG = "HTTP";

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

// --- هندلرهای جدید تشخیص چهره ---

static esp_err_t face_recognize_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/face/recognize received");
    size_t content_len = req->content_len;
    if (content_len == 0 || content_len > 300 * 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid image size");
        return ESP_FAIL;
    }

    uint8_t *jpeg_buf = (uint8_t *)malloc(content_len);
    if (!jpeg_buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // ⭐ اصلاح حیاتی: خواندن کامل داده‌ها در یک حلقه (Loop)
    int total_received = 0;
    int received = 0;
    while (total_received < content_len) {
        received = httpd_req_recv(req, (char *)jpeg_buf + total_received, content_len - total_received);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; // در صورت تایم‌اوت موقت، دوباره تلاش کن
            }
            free(jpeg_buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        total_received += received;
    }

    bool is_unlocked = false;
    int detected_id = -1;
    esp_err_t ret = face_recognition_process(jpeg_buf, total_received, &is_unlocked, &detected_id);
    free(jpeg_buf);

    if (ret == ESP_ERR_NOT_FOUND) {
        httpd_resp_set_status(req, "404 No Face");
        httpd_resp_sendstr(req, "No face detected");
        return ESP_OK;
    } else if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Processing failed");
        return ESP_FAIL;
    }

    if (is_unlocked) {
        char resp[64];
        snprintf(resp, sizeof(resp), "Access Granted! Welcome User ID: %d", detected_id);
        httpd_resp_sendstr(req, resp);
    } else {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Access Denied: Unknown Face");
    }
    return ESP_OK;
}

static esp_err_t face_enroll_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/face/enroll received");
    size_t content_len = req->content_len;
    if (content_len == 0 || content_len > 300 * 1024) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid image size");
        return ESP_FAIL;
    }

    uint8_t *jpeg_buf = (uint8_t *)malloc(content_len);
    if (!jpeg_buf) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Memory allocation failed");
        return ESP_FAIL;
    }

    // ⭐ اصلاح حیاتی: خواندن کامل داده‌ها در یک حلقه
    int total_received = 0;
    int received = 0;
    while (total_received < content_len) {
        received = httpd_req_recv(req, (char *)jpeg_buf + total_received, content_len - total_received);
        if (received <= 0) {
            if (received == HTTPD_SOCK_ERR_TIMEOUT) {
                continue; 
            }
            free(jpeg_buf);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive data");
            return ESP_FAIL;
        }
        total_received += received;
    }

    int new_id = 0;
    esp_err_t ret = face_recognition_enroll(jpeg_buf, total_received, &new_id);
    free(jpeg_buf);

    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Enrollment failed");
        return ESP_FAIL;
    }

    char resp[64];
    snprintf(resp, sizeof(resp), "Face enrolled successfully with ID: %d", new_id);
    httpd_resp_sendstr(req, resp);
    return ESP_OK;
}

static const httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL };
static const httpd_uri_t led_uri = { .uri = "/led", .method = HTTP_GET, .handler = led_handler, .user_ctx = NULL };
static const httpd_uri_t led_toggle_uri = { .uri = "/led/toggle", .method = HTTP_GET, .handler = led_toggle_handler, .user_ctx = NULL };
static const httpd_uri_t face_recognize_uri = { .uri = "/api/face/recognize", .method = HTTP_POST, .handler = face_recognize_handler, .user_ctx = NULL };
static const httpd_uri_t face_enroll_uri = { .uri = "/api/face/enroll", .method = HTTP_POST, .handler = face_enroll_handler, .user_ctx = NULL };

httpd_handle_t http_server_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    config.stack_size = 8192; 
    
    // ⭐ افزایش تایم‌اوت برای جلوگیری از قطع اتصال در حین پردازش سنگین هوش مصنوعی
    config.recv_wait_timeout = 10; // 10 ثانیه
    config.send_wait_timeout = 10; // 10 ثانیه

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