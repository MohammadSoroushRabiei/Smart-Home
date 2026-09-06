#include "http_server.h"
#include "face_recognition.h"
#include "password_manager.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>
#include "app_state.h"


extern const uint8_t servercert_start[] asm("_binary_servercert_pem_start");
extern const uint8_t servercert_end[]   asm("_binary_servercert_pem_end");
extern const uint8_t prvtkey_pem_start[] asm("_binary_prvtkey_pem_start");
extern const uint8_t prvtkey_pem_end[]   asm("_binary_prvtkey_pem_end");

static const char *TAG = "HTTP";

#define FACE_IMAGE_MAX_SIZE     (300 * 1024)
#define FACE_WORKER_STACK_SIZE  16384   // AI decode + inference نیاز به stack نسبتاً بزرگی دارد
#define FACE_WORKER_PRIORITY    3

#define PASSWORD_FORM_MAX_SIZE  256

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
    const char *response = app_state_get_light() ? "LED: ON" : "LED: OFF";
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t led_toggle_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /led/toggle received");
    app_state_set_light(!app_state_get_light());
    const char *response = app_state_get_light() ? "LED: ON" : "LED: OFF";
    return httpd_resp_send(req, response, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t api_status_handler(httpd_req_t *req)
{
    sensor_data_t sensor = app_state_get_sensor_data();

    char buf[192];
    if (sensor.valid) {
        snprintf(buf, sizeof(buf),
                 "{\"light\":%s,\"sensor\":{\"valid\":true,\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f}}",
                 app_state_get_light() ? "true" : "false",
                 sensor.temperature_c, sensor.humidity_percent, sensor.pressure_hpa);
    } else {
        snprintf(buf, sizeof(buf),
                 "{\"light\":%s,\"sensor\":{\"valid\":false}}",
                 app_state_get_light() ? "true" : "false");
    }

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}


static const char *capture_html =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Face Capture</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:420px;margin:20px auto;padding:0 16px;text-align:center;}"
    "video,canvas{width:100%;border-radius:8px;background:#000;}"
    "canvas{display:none;}"
    ".btn-row{display:flex;gap:8px;margin:12px 0;}"
    "button{flex:1;padding:12px;font-size:15px;border-radius:6px;border:none;color:#fff;cursor:pointer;}"
    "#btn-capture{background:#2196F3;}"
    "#btn-retake{background:#888;}"
    "#btn-enroll{background:#4CAF50;}"
    "#btn-recognize{background:#FF9800;}"
    "#btn-flip{background:#607D8B;}"
    "#status{margin-top:10px;font-weight:bold;min-height:24px;}"
    "</style></head><body>"
    "<h2>Face Capture</h2>"
    "<video id=\"video\" autoplay playsinline></video>"
    "<canvas id=\"canvas\" width=\"320\" height=\"240\"></canvas>"
    "<div class=\"btn-row\">"
    "<button id=\"btn-flip\">Flip Camera</button>"
    "<button id=\"btn-capture\">Capture</button>"
    "</div>"
    "<div class=\"btn-row\" id=\"action-row\" style=\"display:none\">"
    "<button id=\"btn-retake\">Retake</button>"
    "<button id=\"btn-enroll\">Enroll</button>"
    "<button id=\"btn-recognize\">Recognize</button>"
    "</div>"
    "<div id=\"status\"></div>"
    "<script>"
    "let stream=null, facingMode='user';"
    "const video=document.getElementById('video');"
    "const canvas=document.getElementById('canvas');"
    "const statusEl=document.getElementById('status');"

    "async function startCamera(){"
    "  if(stream){stream.getTracks().forEach(t=>t.stop());}"
    "  try{"
    "    stream=await navigator.mediaDevices.getUserMedia({video:{facingMode}});"
    "    video.srcObject=stream;"
    "    video.style.display='block';"
    "    canvas.style.display='none';"
    "    document.getElementById('action-row').style.display='none';"
    "    statusEl.textContent='';"
    "  }catch(err){statusEl.textContent='Camera error: '+err.message;}"
    "}"

    "document.getElementById('btn-flip').addEventListener('click',()=>{"
    "  facingMode=(facingMode==='user')?'environment':'user'; startCamera();"
    "});"

    "document.getElementById('btn-capture').addEventListener('click',()=>{"
    "  const ctx=canvas.getContext('2d');"
    "  const tw=320, th=240;"
    "  ctx.fillStyle='#000'; ctx.fillRect(0,0,tw,th);"
    "  const vw=video.videoWidth, vh=video.videoHeight;"
    "  const scale=Math.min(tw/vw, th/vh);"
    "  const dw=vw*scale, dh=vh*scale;"
    "  const dx=(tw-dw)/2, dy=(th-dh)/2;"
    "  ctx.drawImage(video, dx, dy, dw, dh);"
    "  video.style.display='none';"
    "  canvas.style.display='block';"
    "  document.getElementById('action-row').style.display='flex';"
    "});"

    "document.getElementById('btn-retake').addEventListener('click', startCamera);"

    "function sendImage(action){"
    "  statusEl.textContent='Sending...';"
    "  canvas.toBlob((blob)=>{"
    "    fetch('/api/face/'+action,{method:'POST',headers:{'Content-Type':'image/jpeg'},body:blob})"
    "    .then(r=>r.text().then(text=>({status:r.status,text})))"
    "    .then(({status,text})=>{statusEl.textContent='HTTP '+status+': '+text;})"
    "    .catch(err=>{statusEl.textContent='Error: '+err.message;});"
    "  },'image/jpeg',0.85);"
    "}"

    "document.getElementById('btn-enroll').addEventListener('click',()=>sendImage('enroll'));"
    "document.getElementById('btn-recognize').addEventListener('click',()=>sendImage('recognize'));"

    "startCamera();"
    "</script></body></html>";


// صفحه‌ی مستقل تغییر رمز - جدا از صفحه‌ی اصلی، هم‌الگو با /capture
static const char *password_html =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Change Password</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:360px;margin:40px auto;padding:0 16px;}"
    "h2{text-align:center;}"
    "label{display:block;margin:14px 0 4px;font-size:14px;color:#333;}"
    "input{width:100%;padding:10px;font-size:15px;border-radius:6px;border:1px solid #ccc;box-sizing:border-box;}"
    "button{width:100%;margin-top:20px;padding:12px;font-size:16px;border-radius:6px;border:none;background:#2196F3;color:#fff;cursor:pointer;}"
    "#status{margin-top:14px;font-weight:bold;min-height:24px;text-align:center;}"
    "</style></head><body>"
    "<h2>Change Password</h2>"
    "<form id=\"pw-form\">"
    "<label for=\"current\">Current password</label>"
    "<input type=\"password\" id=\"current\" inputmode=\"numeric\">"
    "<label for=\"new\">New password (4-8 chars, letters/digits only)</label>"
    "<input type=\"password\" id=\"new\" inputmode=\"numeric\">"
    "<label for=\"confirm\">Confirm new password</label>"
    "<input type=\"password\" id=\"confirm\" inputmode=\"numeric\">"
    "<button type=\"submit\">Change Password</button>"
    "</form>"
    "<div id=\"status\"></div>"
    "<script>"
    "const alnumRe=/^[A-Za-z0-9]+$/;"
    "document.getElementById('pw-form').addEventListener('submit', async (ev)=>{"
    "  ev.preventDefault();"
    "  const statusEl=document.getElementById('status');"
    "  const current=document.getElementById('current').value;"
    "  const newPw=document.getElementById('new').value;"
    "  const confirm=document.getElementById('confirm').value;"
    "  if(!alnumRe.test(newPw) || newPw.length<4 || newPw.length>8){"
    "    statusEl.textContent='New password must be 4-8 letters/digits only'; return;"
    "  }"
    "  if(newPw!==confirm){ statusEl.textContent='New passwords do not match'; return; }"
    "  statusEl.textContent='Sending...';"
    "  try{"
    "    const body='current='+encodeURIComponent(current)+'&new='+encodeURIComponent(newPw)+'&confirm='+encodeURIComponent(confirm);"
    "    const r=await fetch('/api/password',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});"
    "    const text=await r.text();"
    "    statusEl.textContent='HTTP '+r.status+': '+text;"
    "    if(r.status===200){ document.getElementById('pw-form').reset(); }"
    "  }catch(err){ statusEl.textContent='Error: '+err.message; }"
    "});"
    "</script></body></html>";


static esp_err_t root_handler(httpd_req_t *req)
{
    const char *html =
        "<!DOCTYPE html><html><head><meta charset=\"UTF-8\"><title>Smart Home</title>"
        "<style>"
        "body{font-family:sans-serif;max-width:400px;margin:40px auto;padding:0 16px;}"
        "h1{text-align:center;}"
        ".card{border:1px solid #ddd;border-radius:8px;padding:16px;margin:12px 0;}"
        ".sensor-row{display:flex;justify-content:space-between;margin:6px 0;}"
        "button{width:100%;padding:12px;font-size:16px;border-radius:6px;border:none;background:#2196F3;color:#fff;cursor:pointer;}"
        "</style>"
        "</head><body>"
        "<h1>Smart Home</h1>"

        "<div class=\"card\">"
        "<h2 id=\"led-status\">LED: </h2>"
        "<button id=\"led-toggle\">Toggle LED</button>"
        "</div>"

        "<div class=\"card\">"
        "<h2>Sensor</h2>"
        "<div class=\"sensor-row\"><span>Temperature</span><span id=\"sensor-temp\">--</span></div>"
        "<div class=\"sensor-row\"><span>Humidity</span><span id=\"sensor-hum\">--</span></div>"
        "<div class=\"sensor-row\"><span>Pressure</span><span id=\"sensor-press\">--</span></div>"
        "</div>"

        "<script>"
        "function refreshStatus() {"
        "  fetch(\"/api/status\").then(r => r.json()).then(data => {"
        "    document.getElementById(\"led-status\").textContent = \"LED: \" + (data.light ? \"ON\" : \"OFF\");"
        "    if (data.sensor && data.sensor.valid) {"
        "      document.getElementById(\"sensor-temp\").textContent = data.sensor.temperature.toFixed(1) + \" °C\";"
        "      document.getElementById(\"sensor-hum\").textContent = data.sensor.humidity.toFixed(0) + \" %\";"
        "      document.getElementById(\"sensor-press\").textContent = data.sensor.pressure.toFixed(0) + \" hPa\";"
        "    } else {"
        "      document.getElementById(\"sensor-temp\").textContent = \"N/A\";"
        "      document.getElementById(\"sensor-hum\").textContent = \"N/A\";"
        "      document.getElementById(\"sensor-press\").textContent = \"N/A\";"
        "    }"
        "  });"
        "}"
        "refreshStatus();"
        "setInterval(refreshStatus, 1000);"
        "const btn = document.getElementById(\"led-toggle\");"
        "btn.addEventListener('click', function() {"
        "  fetch(\"/led/toggle\").then(() => refreshStatus());"
        "});"
        "</script></body></html>";
        httpd_resp_set_type(req, "text/html; charset=utf-8");    return httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t password_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, password_html, HTTPD_RESP_USE_STRLEN);
}

// ---------------------------------------------------------------------
// تغییر رمز از طریق وب
// ---------------------------------------------------------------------

// استخراج ساده‌ی value مربوط به یک key از بدنه‌ی application/x-www-form-urlencoded
// عمداً decode نمی‌کند: چون رمز را به alnum محدود کرده‌ایم، کاراکتر خاصی
// (&, =, %) که نیاز به decode داشته باشد در آن مجاز نیست.
static bool extract_form_value(const char *body, const char *key, char *out, size_t out_size)
{
    size_t key_len = strlen(key);
    const char *p = body;

    while (p != NULL && *p != '\0') {
        if (strncmp(p, key, key_len) == 0 && p[key_len] == '=') {
            const char *val_start = p + key_len + 1;
            const char *val_end = strchr(val_start, '&');
            size_t val_len = val_end ? (size_t)(val_end - val_start) : strlen(val_start);

            if (val_len >= out_size) {
                return false;
            }
            memcpy(out, val_start, val_len);
            out[val_len] = '\0';
            return true;
        }
        p = strchr(p, '&');
        if (p) {
            p++;
        }
    }
    return false;
}

static bool is_alnum_str(const char *s)
{
    if (s[0] == '\0') {
        return false;
    }
    for (const char *c = s; *c != '\0'; c++) {
        if (!isalnum((unsigned char)*c)) {
            return false;
        }
    }
    return true;
}

static esp_err_t api_password_handler(httpd_req_t *req)
{
    if (req->content_len == 0 || req->content_len > PASSWORD_FORM_MAX_SIZE) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid request size");
        return ESP_OK;
    }

    char body[PASSWORD_FORM_MAX_SIZE + 1];
    size_t to_read = req->content_len;
    int received = httpd_req_recv(req, body, to_read);
    if (received <= 0) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to read body");
        return ESP_OK;
    }
    body[received] = '\0';

    char current[PASSWORD_MAX_LEN + 1] = {0};
    char new_pw[PASSWORD_MAX_LEN + 1] = {0};
    char confirm[PASSWORD_MAX_LEN + 1] = {0};

    if (!extract_form_value(body, "current", current, sizeof(current)) ||
        !extract_form_value(body, "new", new_pw, sizeof(new_pw)) ||
        !extract_form_value(body, "confirm", confirm, sizeof(confirm))) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Missing or too long fields");
        return ESP_OK;
    }

    if (!password_manager_verify(current)) {
        ESP_LOGW(TAG, "Password change rejected: current password incorrect");
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Current password incorrect");
        return ESP_OK;
    }

    if (strcmp(new_pw, confirm) != 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "New passwords do not match");
        return ESP_OK;
    }

    if (!is_alnum_str(new_pw)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Password must contain only letters and digits");
        return ESP_OK;
    }

    esp_err_t ret = password_manager_set(new_pw);
    if (ret != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Password must be 4-8 characters");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Password changed successfully via web");
    httpd_resp_sendstr(req, "Password changed successfully");
    return ESP_OK;
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


static esp_err_t capture_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, capture_html, HTTPD_RESP_USE_STRLEN);
}

// ---------------------------------------------------------------------
// ثبت URI ها و راه‌اندازی سرور
// ---------------------------------------------------------------------

static const httpd_uri_t root_uri = { .uri = "/", .method = HTTP_GET, .handler = root_handler, .user_ctx = NULL };
static const httpd_uri_t led_uri = { .uri = "/led", .method = HTTP_GET, .handler = led_handler, .user_ctx = NULL };
static const httpd_uri_t led_toggle_uri = { .uri = "/led/toggle", .method = HTTP_GET, .handler = led_toggle_handler, .user_ctx = NULL };
static const httpd_uri_t api_status_uri = { .uri = "/api/status", .method = HTTP_GET, .handler = api_status_handler, .user_ctx = NULL };
static const httpd_uri_t face_recognize_uri = { .uri = "/api/face/recognize", .method = HTTP_POST, .handler = face_recognize_handler, .user_ctx = NULL };
static const httpd_uri_t face_enroll_uri = { .uri = "/api/face/enroll", .method = HTTP_POST, .handler = face_enroll_handler, .user_ctx = NULL };
static const httpd_uri_t capture_page_uri = { .uri = "/capture", .method = HTTP_GET, .handler = capture_page_handler, .user_ctx = NULL };
static const httpd_uri_t password_page_uri = { .uri = "/password", .method = HTTP_GET, .handler = password_page_handler, .user_ctx = NULL };
static const httpd_uri_t api_password_uri = { .uri = "/api/password", .method = HTTP_POST, .handler = api_password_handler, .user_ctx = NULL };




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
    httpd_ssl_config_t config = HTTPD_SSL_CONFIG_DEFAULT();

    config.servercert = servercert_start;
    config.servercert_len = servercert_end - servercert_start;
    config.prvtkey_pem = prvtkey_pem_start;
    config.prvtkey_len = prvtkey_pem_end - prvtkey_pem_start;

    config.httpd.stack_size        = 8192;
    config.httpd.recv_wait_timeout = 10;
    config.httpd.send_wait_timeout = 10;
    config.httpd.max_uri_handlers  = 12;  // پیش‌فرض ۸ کافی نبود؛ الان ۹ هندلر داریم + کمی فضای رشد

    esp_err_t err = httpd_ssl_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTPS server: %s", esp_err_to_name(err));
        return NULL;
    }

    static const struct { const httpd_uri_t *uri; const char *name; } handlers[] = {
        { &root_uri,            "/" },
        { &led_uri,              "/led" },
        { &led_toggle_uri,       "/led/toggle" },
        { &api_status_uri,       "/api/status" },
        { &face_recognize_uri,   "/api/face/recognize" },
        { &face_enroll_uri,      "/api/face/enroll" },
        { &capture_page_uri,     "/capture" },
        { &password_page_uri,    "/password" },
        { &api_password_uri,     "/api/password" },
    };
    for (size_t i = 0; i < sizeof(handlers) / sizeof(handlers[0]); i++) {
        esp_err_t reg_err = httpd_register_uri_handler(server, handlers[i].uri);
        if (reg_err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to register handler for %s: %s", handlers[i].name, esp_err_to_name(reg_err));
        }
    }


    ESP_LOGI(TAG, "HTTPS server started");
    return server;
}