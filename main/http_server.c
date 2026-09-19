#include "http_server.h"
#include "face_recognition.h"
#include "password_manager.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "esp_system.h"
#include "led.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include "app_state.h"
#include "mqtt_manager.h"
#include "mqtt_config.h"
#include "enroll_token.h"
#include "face_db.h"
#include "ml_agent.h"
#include "wifi_manager.h"
#include "virtual_devices.h"

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
    FACE_OP_DELETE,   // حذف همه‌ی نمونه‌های یک شخص (داشبورد وب) - از صف کارگر
                      // می‌رود تا با recognize/enroll روی همان تشخیص‌دهنده سریالایز شود
} face_op_t;

typedef struct {
    httpd_req_t *req;
    face_op_t op;
    char name[FACE_DB_NAME_MAX_LEN + 1];
} face_work_item_t;

static QueueHandle_t s_face_queue = NULL;

// ---------------------------------------------------------------------
// نشست وبِ تنظیمات - توکن تصادفی فقط در RAM؛ با هر ورودِ موفق توکن جدید
// ساخته و قبلی باطل می‌شود. فقط POST /api/settings/unlock توکن می‌سازد و
// به‌صورت کوکی Secure برمی‌گرداند (خروج آشکار نداریم؛ انقضا یا بستن
// مرورگر کافی است - هم‌الگوی توکن Enroll).
// ---------------------------------------------------------------------
#define SETTINGS_SESSION_TTL_US  ((int64_t)10 * 60 * 1000 * 1000)
#define SETTINGS_TOKEN_BYTES     16
#define SETTINGS_COOKIE_NAME     "shs"

static char s_settings_session[SETTINGS_TOKEN_BYTES * 2 + 1];
static int64_t s_settings_session_expiry_us = 0;

// افراد ثبت‌شده (فقط برای خواندن و ساخت JSON) و بافر پاسخ تنظیمات - سرور
// تک‌نخی است و هندلرها همزمان اجرا نمی‌شوند، static بودن مشکلی ندارد
static face_db_person_t s_persons[FACE_DB_MAX_ENTRIES];
static char s_json_buf[3072];

static void settings_session_create(void)
{
    uint8_t raw[SETTINGS_TOKEN_BYTES];
    esp_fill_random(raw, sizeof(raw));
    for (size_t i = 0; i < SETTINGS_TOKEN_BYTES; i++) {
        snprintf(&s_settings_session[i * 2], 3, "%02x", raw[i]);
    }
    s_settings_session_expiry_us = esp_timer_get_time() + SETTINGS_SESSION_TTL_US;
}

static bool settings_session_valid(httpd_req_t *req)
{
    if (esp_timer_get_time() > s_settings_session_expiry_us) {
        return false;
    }
    char cookie[SETTINGS_TOKEN_BYTES * 2 + 1];
    size_t cookie_len = sizeof(cookie);
    if (httpd_req_get_cookie_val(req, SETTINGS_COOKIE_NAME, cookie, &cookie_len) != ESP_OK) {
        return false;
    }
    return strcmp(cookie, s_settings_session) == 0;
}

// ---------------------------------------------------------------------
// ابزارهای مشترک داشبورد وب
// ---------------------------------------------------------------------

static esp_err_t send_ok_json(httpd_req_t *req, const char *json)
{
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_sendstr(req, json);
}

// خواندن بدنه‌ی کوچک (فرم urlencoded) - الگوی مشترک هندلرهای POST داشبورد
static esp_err_t read_form_body(httpd_req_t *req, char *body, size_t body_size)
{
    if (req->content_len == 0 || (size_t)req->content_len >= body_size) {
        return ESP_ERR_INVALID_SIZE;
    }
    int received = httpd_req_recv(req, body, req->content_len);
    if (received <= 0) {
        return ESP_FAIL;
    }
    body[received] = '\0';
    return ESP_OK;
}

static void send_unauthorized(httpd_req_t *req)
{
    httpd_resp_set_status(req, "401 Unauthorized");
    send_ok_json(req, "{\"ok\":false,\"error\":\"Session expired - log in again\"}");
}

// کپی رشته با escape کاراکترهای خاص JSON - برای SSID و hostname و نام چهره
// که از ورودی کاربر می‌آیند و می‌توانند کوتیشن/بک‌اسلش داشته باشند
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

static const char *mqtt_state_str(mqtt_manager_state_t state)
{
    switch (state) {
        case MQTT_MGR_STATE_UNCONFIGURED: return "UNCONFIGURED";
        case MQTT_MGR_STATE_DISABLED:     return "DISABLED";
        case MQTT_MGR_STATE_CONNECTING:   return "CONNECTING";
        case MQTT_MGR_STATE_CONNECTED:    return "CONNECTED";
        default:                          return "UNKNOWN";
    }
}

// آرایه‌ی JSON افراد ثبت‌شده (با براکت‌ها) در out می‌نویسد و تعداد بایت
// نوشته‌شده را برمی‌گرداند؛ در صورت کمبود جا امن کوتاه می‌شود
static size_t faces_json(char *out, size_t out_size)
{
    size_t n = face_db_get_persons(s_persons, FACE_DB_MAX_ENTRIES);
    size_t off = 0;
    int wn = snprintf(out, out_size, "[");
    if (wn < 0) {
        return 0;
    }
    off = (size_t)wn;
    for (size_t i = 0; i < n && off + 2 < out_size; i++) {
        char name_esc[2 * FACE_DB_NAME_MAX_LEN + 2];
        json_escape(s_persons[i].name, name_esc, sizeof(name_esc));
        wn = snprintf(out + off, out_size - off,
                      "%s{\"name\":\"%s\",\"samples\":%u}",
                      i > 0 ? "," : "", name_esc, (unsigned)s_persons[i].sample_count);
        if (wn < 0) {
            break;
        }
        off = ((size_t)wn >= out_size - off) ? out_size - 1 : off + (size_t)wn;
    }
    if (off + 1 < out_size) {
        off += (size_t)snprintf(out + off, out_size - off, "]");
    } else {
        off = out_size - 1;
    }
    out[off] = '\0';
    return off;
}

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

    // اگر شبیه‌ساز هنوز آماده نیست، همان پیش‌فرض محافظه‌کارانه‌ی ml_agent
    bool presence = true;
    float lux = 5.0f;
    virtual_devices_get_env(&presence, &lux);

    ml_agent_stats_t ml;
    ml_agent_get_stats(&ml);

    char mqtt_host[MQTT_CONFIG_HOST_MAX_LEN + 1];
    bool have_mqtt_host = mqtt_config_get_host(mqtt_host, sizeof(mqtt_host));

    char sensor_json[128];
    if (sensor.valid) {
        snprintf(sensor_json, sizeof(sensor_json),
                 "\"valid\":true,\"temperature\":%.1f,\"humidity\":%.1f,\"pressure\":%.1f",
                 sensor.temperature_c, sensor.humidity_percent, sensor.pressure_hpa);
    } else {
        snprintf(sensor_json, sizeof(sensor_json), "\"valid\":false");
    }

    bool wifi_conn = wifi_is_connected();
    char ssid_esc[67];
    char host_esc[2 * MQTT_CONFIG_HOST_MAX_LEN + 2];
    json_escape(wifi_conn ? wifi_get_connected_ssid() : "", ssid_esc, sizeof(ssid_esc));
    json_escape(have_mqtt_host ? mqtt_host : "", host_esc, sizeof(host_esc));

    char buf[640];
    snprintf(buf, sizeof(buf),
             "{\"light\":%s,\"fan\":%s,\"lock\":%s,"
             "\"sensor\":{%s},"
             "\"presence\":%s,\"lux\":%.1f,"
             "\"wifi\":{\"connected\":%s,\"ssid\":\"%s\",\"ip\":\"%s\"},"
             "\"mqtt\":{\"state\":\"%s\",\"host\":\"%s\"},"
             "\"ml\":{\"auto\":%s,\"p_light\":%.2f,\"p_fan\":%.2f,"
             "\"acc_light\":%.0f,\"acc_fan\":%.0f,\"n_light\":%u,\"n_fan\":%u}}",
             app_state_get_light() ? "true" : "false",
             app_state_get_fan() ? "true" : "false",
             app_state_get_lock() ? "true" : "false",
             sensor_json,
             presence ? "true" : "false", lux,
             wifi_conn ? "true" : "false", ssid_esc,
             wifi_conn ? wifi_get_ip_str() : "",
             mqtt_state_str(mqtt_manager_get_state()), host_esc,
             ml.any_auto ? "true" : "false",
             ml.p_light, ml.p_fan,
             ml.acc_light, ml.acc_fan,
             (unsigned)ml.n_light, (unsigned)ml.n_fan);

    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
}

// صفحه‌ی Recognize - همیشه باز، بدون نیاز به توکن. عیناً همان تجربه‌ی مودال
// Face ID داخل داشبورد وب است (تک‌دکمه‌ای: Capture and Verify) تا QR زیر
// دکمه‌ی Unlock روی LCD هم دقیقاً همان ظاهر و رفتار را بدهد.
static const char *recognize_html =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Face ID</title>"
    "<style>"
    ":root{--bg:#111418;--card:#1b2027;--line:#2a313b;--text:#e8eaed;--muted:#8b95a1;"
    "--accent:#4aa3ff;--green:#34c26b;--orange:#ff9d42;--red:#ff5c5c}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:var(--bg);"
    "color:var(--text);max-width:420px;margin:0 auto;padding:14px 14px 40px}"
    "h1{font-size:18px}"
    ".row{display:flex;align-items:center;justify-content:space-between;gap:8px}"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-top:12px}"
    "video,canvas{width:100%;border-radius:10px;background:#000;}"
    "canvas{display:none;}"
    ".btn{border:none;border-radius:10px;padding:12px 16px;font-size:14px;font-weight:600;color:#fff;"
    "background:var(--accent);cursor:pointer;}"
    ".btn.secondary{background:#2c3540;}"
    ".btn:disabled{opacity:.5;cursor:not-allowed;}"
    ".btn-row{display:flex;gap:8px;margin:10px 0 0;}"
    ".btn-row .btn{flex:1;}"
    ".small{font-size:12px;color:var(--muted);}"
    "#status{margin-top:12px;font-weight:700;min-height:22px;}"
    "</style></head><body>"
    "<div class=\"row\"><h1>Face ID</h1>"
    "<button class=\"btn secondary\" id=\"btn-close\" style=\"width:auto;padding:8px 14px\">Close</button></div>"
    "<p class=\"small\">Look at the camera, then capture and verify to unlock the door.</p>"
    "<div class=\"card\">"
    "<video id=\"video\" autoplay playsinline></video>"
    "<canvas id=\"canvas\" width=\"320\" height=\"240\"></canvas>"
    "<div class=\"btn-row\">"
    "<button class=\"btn secondary\" id=\"btn-flip\">Flip</button>"
    "<button class=\"btn\" id=\"btn-scan\">Capture and Verify</button>"
    "</div>"
    "<div id=\"status\"></div>"
    "</div>"
    "<script>"
    "let stream=null, facingMode='user';"
    "const video=document.getElementById('video');"
    "const canvas=document.getElementById('canvas');"
    "const statusEl=document.getElementById('status');"
    "const btnScan=document.getElementById('btn-scan');"

    "async function startCamera(){"
    "  if(stream){stream.getTracks().forEach(t=>t.stop());}"
    "  try{"
    "    stream=await navigator.mediaDevices.getUserMedia({video:{facingMode}});"
    "    video.srcObject=stream;"
    "    video.style.display='block';"
    "  }catch(err){statusEl.style.color='var(--red)';statusEl.textContent='Camera error: '+err.message;}"
    "}"

    "document.getElementById('btn-flip').addEventListener('click',()=>{"
    "  facingMode=(facingMode==='user')?'environment':'user'; startCamera();"
    "});"

    "document.getElementById('btn-close').addEventListener('click',()=>{"
    "  window.location.href='/';"
    "});"

    "btnScan.addEventListener('click',()=>{"
    "  if(!video.videoWidth){statusEl.textContent='Camera not ready';return;}"
    "  btnScan.disabled=true;"
    "  statusEl.style.color='var(--muted)';"
    "  statusEl.textContent='Verifying...';"
    "  const ctx=canvas.getContext('2d');"
    "  ctx.fillStyle='#000'; ctx.fillRect(0,0,320,240);"
    "  const s=Math.min(320/video.videoWidth, 240/video.videoHeight);"
    "  ctx.drawImage(video, (320-video.videoWidth*s)/2, (240-video.videoHeight*s)/2,"
    "                 video.videoWidth*s, video.videoHeight*s);"
    "  canvas.toBlob((blob)=>{"
    "    fetch('/api/face/recognize',{method:'POST',headers:{'Content-Type':'image/jpeg'},body:blob})"
    "    .then(r=>r.text().then(text=>({status:r.status,text})))"
    "    .then(({status,text})=>{"
    "      statusEl.style.color=(status===200)?'var(--green)':'var(--red)';"
    "      statusEl.textContent=(status===200?'OK: ':'')+text;"
    "      btnScan.disabled=false;"
    "    })"
    "    .catch(err=>{statusEl.style.color='var(--red)';statusEl.textContent='Error: '+err.message;"
    "                 btnScan.disabled=false;});"
    "  },'image/jpeg',0.85);"
    "});"

    "startCamera();"
    "</script></body></html>";


// صفحه‌ی Enroll چند-نمونه‌ای - پشت توکن یک‌بارمصرف؛ توکن از query string خوانده
// و به هر درخواست POST پیوست می‌شود؛ اگر نامعتبر باشد سرور با 403 رد می‌کند.
// نام یک‌بار وارد می‌شود و هر «Add Sample» یک نمونه‌ی جدید زیر همان نام ثبت
// می‌کند؛ توکن تا پایان مهلت خود چندبارمصرف است (تصمیم عمدی ماژول enroll_token)
static const char *enroll_html =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Face Enrollment</title>"
    "<style>"
    ":root{--bg:#111418;--card:#1b2027;--line:#2a313b;--text:#e8eaed;--muted:#8b95a1;"
    "--accent:#4aa3ff;--green:#34c26b;--orange:#ff9d42;--red:#ff5c5c}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:var(--bg);"
    "color:var(--text);max-width:420px;margin:0 auto;padding:14px 14px 40px}"
    "h1{font-size:19px;margin-bottom:2px}"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-top:12px}"
    "video,canvas{width:100%;border-radius:10px;background:#000;}"
    "canvas{display:none;}"
    ".btn{border:none;border-radius:10px;padding:12px 16px;font-size:14px;font-weight:600;color:#fff;"
    "background:var(--accent);cursor:pointer;}"
    ".btn.secondary{background:#2c3540;}"
    ".btn.green{background:var(--green);}"
    ".btn.orange{background:var(--orange);}"
    ".btn:disabled{opacity:.5;cursor:not-allowed;}"
    ".btn-row{display:flex;gap:8px;margin:10px 0 0;}"
    ".btn-row .btn{flex:1;}"
    ".small{font-size:12px;color:var(--muted);}"
    "#status{margin-top:12px;font-weight:700;min-height:22px;}"
    "#sample-status{margin-top:8px;}"
    "#name-input{width:100%;padding:11px 12px;border-radius:10px;border:1px solid var(--line);"
    "background:#12161b;color:var(--text);font-size:15px;box-sizing:border-box;}"
    "a{color:var(--accent);text-decoration:none;font-size:13px;}"
    "</style></head><body>"
    "<a href=\"/\">&larr; Dashboard</a>"
    "<h1>Face Enrollment</h1>"
    "<p class=\"small\">Enter a name, capture 3+ samples of the face, then tap Done.</p>"
    "<div class=\"card\">"
    "<input type=\"text\" id=\"name-input\" placeholder=\"Full name\" maxlength=\"22\">"
    "<div id=\"sample-status\" class=\"small\">Samples added: 0 (3+ recommended)</div>"
    "<video id=\"video\" autoplay playsinline></video>"
    "<canvas id=\"canvas\" width=\"320\" height=\"240\"></canvas>"
    "<div class=\"btn-row\">"
    "<button class=\"btn secondary\" id=\"btn-flip\">Flip</button>"
    "<button class=\"btn\" id=\"btn-capture\">Capture</button>"
    "</div>"
    "<div class=\"btn-row\" id=\"action-row\" style=\"display:none\">"
    "<button class=\"btn secondary\" id=\"btn-retake\">Retake</button>"
    "<button class=\"btn green\" id=\"btn-add\">Add Sample</button>"
    "</div>"
    "<div class=\"btn-row\" id=\"done-row\" style=\"display:none\">"
    "<button class=\"btn orange\" id=\"btn-done\">Done</button>"
    "</div>"
    "<div class=\"btn-row\" id=\"close-row\" style=\"display:none\">"
    "<button class=\"btn secondary\" id=\"btn-close\">Close</button>"
    "</div>"
    "<div id=\"status\"></div>"
    "</div>"
    "<script>"
    "let stream=null, facingMode='user', samples=0, busy=false;"
    "const video=document.getElementById('video');"
    "const canvas=document.getElementById('canvas');"
    "const statusEl=document.getElementById('status');"
    "const sampleEl=document.getElementById('sample-status');"
    "const params=new URLSearchParams(window.location.search);"
    "const token=params.get('token')||'';"

    "const nameInput=document.getElementById('name-input');"
    "const btnAdd=document.getElementById('btn-add');"
    "const btnDone=document.getElementById('btn-done');"

    "function setSampleText(){"
    "  sampleEl.textContent='Samples added: '+samples+' (3+ recommended)';"
    "}"

    "function updateAddState(){"
    "  const hasName = nameInput.value.trim().length>0;"
    "  const hasCapture = canvas.style.display==='block';"
    "  btnAdd.disabled = busy || !(hasName && hasCapture);"
    "}"

    "nameInput.addEventListener('input', updateAddState);"

    "async function startCamera(){"
    "  if(stream){stream.getTracks().forEach(t=>t.stop());}"
    "  try{"
    "    stream=await navigator.mediaDevices.getUserMedia({video:{facingMode}});"
    "    video.srcObject=stream;"
    "    video.style.display='block';"
    "    canvas.style.display='none';"
    "    document.getElementById('action-row').style.display='none';"
    "    updateAddState();"
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
    "  updateAddState();"
    "});"

    "document.getElementById('btn-retake').addEventListener('click', startCamera);"

    "btnAdd.addEventListener('click',()=>{"
    "  if(btnAdd.disabled) return;"
    "  busy=true; updateAddState();"
    "  statusEl.style.color='var(--muted)';"
    "  statusEl.textContent='Sending sample '+(samples+1)+'...';"
    "  canvas.toBlob((blob)=>{"
    "    const name=encodeURIComponent(nameInput.value.trim());"
    "    fetch('/api/face/enroll?token='+encodeURIComponent(token)+'&name='+name,{method:'POST',headers:{'Content-Type':'image/jpeg'},body:blob})"
    "    .then(r=>r.text().then(text=>({status:r.status,text})))"
    "    .then(({status,text})=>{"
    "      busy=false;"
    "      if(status===200){"
    "        samples++; setSampleText();"
    "        document.getElementById('done-row').style.display='flex';"
    "        statusEl.style.color='var(--green)';"
    "        statusEl.textContent='Sample '+samples+' added.';"
    "        startCamera();"
    "      } else {"
    "        statusEl.style.color='var(--red)';"
    "        statusEl.textContent=text;"
    "        updateAddState();"
    "      }"
    "    })"
    "    .catch(err=>{busy=false; statusEl.textContent='Error: '+err.message; updateAddState();});"
    "  },'image/jpeg',0.85);"
    "});"

    "btnDone.addEventListener('click',()=>{"
    "  if(samples===0) return;"
    "  statusEl.style.color='var(--green)';"
    "  statusEl.textContent='Enrollment complete: '+samples+' sample(s) for '+nameInput.value.trim()+'.';"
    "  samples=0; setSampleText();"
    "  document.getElementById('done-row').style.display='none';"
    "  document.getElementById('close-row').style.display='flex';"
    "  nameInput.value='';"
    "  updateAddState();"
    "});"

    "document.getElementById('btn-close').addEventListener('click',()=>{"
    "  window.location.href='/';"
    "});"

    "startCamera();"
    "</script></body></html>";


// صفحه‌ی مستقل تغییر رمز - جدا از صفحه‌ی اصلی، هم‌الگو با /capture
// از این نسخه به بعد، رمز قفل درب و رمز منوی تنظیمات دو رمز مستقل هستند؛
// کاربر باید مشخص کند کدام‌یک را می‌خواهد تغییر دهد (فیلد "type").
static const char *password_html =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Change Password</title>"
    "<style>"
    "body{font-family:sans-serif;max-width:360px;margin:40px auto;padding:0 16px;}"
    "h2{text-align:center;}"
    "label{display:block;margin:14px 0 4px;font-size:14px;color:#333;}"
    "input,select{width:100%;padding:10px;font-size:15px;border-radius:6px;border:1px solid #ccc;box-sizing:border-box;}"
    "button{width:100%;margin-top:20px;padding:12px;font-size:16px;border-radius:6px;border:none;background:#2196F3;color:#fff;cursor:pointer;}"
    "#status{margin-top:14px;font-weight:bold;min-height:24px;text-align:center;}"
    "</style></head><body>"
    "<h2>Change Password</h2>"
    "<form id=\"pw-form\">"
    "<label for=\"type\">Which password?</label>"
    "<select id=\"type\">"
    "<option value=\"settings\">Settings Menu Password</option>"
    "<option value=\"lock\">Door Unlock Password</option>"
    "</select>"
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
    "  const type=document.getElementById('type').value;"
    "  const current=document.getElementById('current').value;"
    "  const newPw=document.getElementById('new').value;"
    "  const confirm=document.getElementById('confirm').value;"
    "  if(!alnumRe.test(newPw) || newPw.length<4 || newPw.length>8){"
    "    statusEl.textContent='New password must be 4-8 letters/digits only'; return;"
    "  }"
    "  if(newPw!==confirm){ statusEl.textContent='New passwords do not match'; return; }"
    "  statusEl.textContent='Sending...';"
    "  try{"
    "    const body='type='+encodeURIComponent(type)+'&current='+encodeURIComponent(current)+'&new='+encodeURIComponent(newPw)+'&confirm='+encodeURIComponent(confirm);"
    "    const r=await fetch('/api/password',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body});"
    "    const text=await r.text();"
    "    statusEl.textContent='HTTP '+r.status+': '+text;"
    "    if(r.status===200){ document.getElementById('pw-form').reset(); }"
    "  }catch(err){ statusEl.textContent='Error: '+err.message; }"
    "});"
    "</script></body></html>";


// تعریف کامل این رشته پایین‌تر از اینجاست (بعد از هندلر رمز)؛ root_handler
// که بالاتر در فایل است به آن اشاره می‌کند
static const char *dashboard_html;

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, dashboard_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t password_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, password_html, HTTPD_RESP_USE_STRLEN);
}

// ---------------------------------------------------------------------
// تغییر رمز از طریق وب
// ---------------------------------------------------------------------

static void url_decode(char *dst, const char *src, size_t dst_size)
{
    size_t di = 0;
    while (*src != '\0' && di + 1 < dst_size) {
        if (*src == '%' && src[1] != '\0' && src[2] != '\0') {
            char hex[3] = { src[1], src[2], '\0' };
            dst[di++] = (char)strtol(hex, NULL, 16);
            src += 3;
        } else if (*src == '+') {
            dst[di++] = ' ';
            src++;
        } else {
            dst[di++] = *src++;
        }
    }
    dst[di] = '\0';
}


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

// نگاشت مقدار فیلد "type" فرم به نوع رمز؛ هر مقدار نامعتبر یا خالی
// (مثلاً از یک کلاینت قدیمی‌تر که این فیلد را نمی‌فرستاد) به‌طور محافظه‌کارانه
// رمز منوی تنظیمات در نظر گرفته می‌شود، نه رمز قفل درب.
static password_kind_t parse_password_kind(const char *type_str)
{
    if (type_str != NULL && strcmp(type_str, "lock") == 0) {
        return PASSWORD_KIND_LOCK;
    }
    return PASSWORD_KIND_SETTINGS;
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

    char type_str[16] = {0};
    char current[PASSWORD_MAX_LEN + 1] = {0};
    char new_pw[PASSWORD_MAX_LEN + 1] = {0};
    char confirm[PASSWORD_MAX_LEN + 1] = {0};

    // فیلد "type" اختیاری است (سازگاری با کلاینت‌های قدیمی‌تر) - نبودش
    // یعنی رمز منوی تنظیمات در نظر گرفته می‌شود
    extract_form_value(body, "type", type_str, sizeof(type_str));

    if (!extract_form_value(body, "current", current, sizeof(current)) ||
        !extract_form_value(body, "new", new_pw, sizeof(new_pw)) ||
        !extract_form_value(body, "confirm", confirm, sizeof(confirm))) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Missing or too long fields");
        return ESP_OK;
    }

    password_kind_t kind = parse_password_kind(type_str);

    if (!password_manager_verify(kind, current)) {
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

    esp_err_t ret = password_manager_set(kind, new_pw);
    if (ret != ESP_OK) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Password must be 4-8 characters");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Password (%s) changed successfully via web",
             kind == PASSWORD_KIND_SETTINGS ? "settings" : "lock");
    httpd_resp_sendstr(req, "Password changed successfully");
    return ESP_OK;
}

// ---------------------------------------------------------------------
// داشبورد وب - صفحه‌ی اصلی (جایگزین صفحه‌ی تست قدیمی روت). خودکفا است:
// بدون CDN و فایل خارجی تا روی شبکه‌ی محلی بدون اینترنت هم کامل کار کند.
// کنترل‌ها همه از مسیر رسمی app_state می‌روند تا publish به HA، آپدیت LCD
// و یادگیری ML خودشان اتفاق بیفتد.
// ---------------------------------------------------------------------
static const char *dashboard_html =
    "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
    "<title>Smart Home</title>"
    "<style>"
    ":root{--bg:#111418;--card:#1b2027;--line:#2a313b;--text:#e8eaed;--muted:#8b95a1;"
    "--accent:#4aa3ff;--green:#34c26b;--orange:#ff9d42;--red:#ff5c5c;--cyan:#39c5cf}"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:-apple-system,'Segoe UI',Roboto,sans-serif;background:var(--bg);"
    "color:var(--text);max-width:520px;margin:0 auto;padding:14px 14px 40px}"
    "header{display:flex;align-items:center;justify-content:space-between;margin-bottom:12px;gap:8px}"
    "h1{font-size:19px}"
    "#net{display:flex;gap:6px;align-items:center;font-size:11px;color:var(--muted)}"
    ".chip{background:var(--card);border:1px solid var(--line);border-radius:999px;padding:3px 10px;white-space:nowrap}"
    ".chip b{color:var(--text)}"
    "#dot{width:9px;height:9px;border-radius:50%;background:var(--green);display:inline-block}"
    ".card{background:var(--card);border:1px solid var(--line);border-radius:14px;padding:16px;margin-bottom:12px}"
    ".card h2{font-size:13px;color:var(--muted);font-weight:600;margin-bottom:12px;letter-spacing:.5px;text-transform:uppercase}"
    ".row{display:flex;align-items:center;justify-content:space-between;gap:10px}"
    ".dev{display:flex;align-items:center;gap:12px}"
    ".icon{width:42px;height:42px;border-radius:12px;display:flex;align-items:center;justify-content:center;font-size:20px;background:#232a33;flex:none}"
    ".switch{position:relative;width:54px;height:30px;flex:none}"
    ".switch input{opacity:0;width:100%;height:100%;position:absolute;margin:0;cursor:pointer;z-index:2}"
    ".slider{position:absolute;inset:0;background:#39414c;border-radius:999px;transition:.2s}"
    ".slider:before{content:'';position:absolute;width:22px;height:22px;border-radius:50%;background:#fff;top:4px;left:4px;transition:.2s}"
    ".switch input:checked+.slider{background:var(--green)}"
    ".switch input:checked+.slider:before{transform:translateX(24px)}"
    ".btn{border:none;border-radius:10px;padding:12px 16px;font-size:14px;font-weight:600;color:#fff;"
    "background:var(--accent);cursor:pointer;width:100%}"
    ".btn.secondary{background:#2c3540}"
    ".btn.danger{background:var(--red)}"
    ".btn:disabled{opacity:.5}"
    "input[type=password],input[type=text]{width:100%;padding:11px 12px;border-radius:10px;"
    "border:1px solid var(--line);background:#12161b;color:var(--text);font-size:15px}"
    ".badge{font-size:12px;font-weight:700;padding:4px 10px;border-radius:999px}"
    ".badge.locked{background:#33241a;color:var(--orange)}"
    ".badge.unlocked{background:#1d3a28;color:var(--green)}"
    ".small{font-size:12px;color:var(--muted)}"
    ".env-grid{display:grid;grid-template-columns:repeat(3,1fr);gap:10px}"
    ".env-item{background:#12161b;border:1px solid var(--line);border-radius:10px;padding:10px;text-align:center}"
    ".env-item .v{font-size:16px;font-weight:700}"
    ".env-item .l{font-size:11px;color:var(--muted);margin-top:2px}"
    ".ml-rows{display:flex;flex-direction:column;gap:8px;font-size:13px;margin-top:12px}"
    ".ml-rows .row span:last-child{font-weight:700}"
    ".mode-pill{padding:7px 16px;border-radius:999px;font-weight:700;font-size:12px;border:none;color:#fff;"
    "cursor:pointer;background:var(--orange);flex:none}"
    ".mode-pill.auto{background:var(--accent)}"
    ".overlay{position:fixed;inset:0;background:rgba(10,12,15,.97);z-index:10;display:none;overflow-y:auto;padding:18px}"
    ".overlay.open{display:block}"
    ".set-section{margin-bottom:14px}"
    "video{width:100%;border-radius:10px;background:#000}"
    ".face-item{display:flex;justify-content:space-between;align-items:center;background:#12161b;"
    "border:1px solid var(--line);border-radius:10px;padding:10px 12px;margin-bottom:6px;gap:8px}"
    "#toast{position:fixed;bottom:18px;left:50%;transform:translateX(-50%);background:#2c3540;color:#fff;"
    "padding:10px 18px;border-radius:10px;font-size:13px;opacity:0;transition:.3s;pointer-events:none;z-index:50}"
    "#toast.show{opacity:1}"
    ".status-line{min-height:18px;font-size:13px;margin-top:10px;color:var(--muted)}"
    "a{color:var(--accent)}"
    "</style></head><body>"
    "<header><h1>Smart Home</h1>"
    "<div id=\"net\"><span id=\"dot\"></span>"
    "<span class=\"chip\" id=\"chip-ip\">offline</span>"
    "<span class=\"chip\">MQTT <b id=\"chip-mqtt\">-</b></span></div></header>"

    "<div class=\"card\"><div class=\"row\">"
    "<div class=\"dev\"><div class=\"icon\">&#128161;</div><div><b>Light</b><div class=\"small\">Living room</div></div></div>"
    "<div class=\"switch\"><input type=\"checkbox\" id=\"sw-light\"><label class=\"slider\" for=\"sw-light\"></label></div>"
    "</div></div>"

    "<div class=\"card\"><div class=\"row\">"
    "<div class=\"dev\"><div class=\"icon\">&#127744;</div><div><b>Fan</b><div class=\"small\">Ceiling fan</div></div></div>"
    "<div class=\"switch\"><input type=\"checkbox\" id=\"sw-fan\"><label class=\"slider\" for=\"sw-fan\"></label></div>"
    "</div></div>"

    "<div class=\"card\"><h2>Door Lock</h2>"
    "<div class=\"row\"><div class=\"dev\"><div class=\"icon\">&#128682;</div>"
    "<div><span id=\"lock-state\" class=\"badge locked\">LOCKED</span>"
    "<div class=\"small\">Auto-relocks after 5 s</div></div></div></div>"
    "<div style=\"display:flex;gap:8px;margin-top:12px\">"
    "<input type=\"password\" id=\"lock-pin\" inputmode=\"numeric\" maxlength=\"8\" placeholder=\"Door PIN\">"
    "<button class=\"btn\" id=\"btn-unlock\" style=\"width:auto;flex:none\">Unlock</button></div>"
    "<button class=\"btn secondary\" id=\"btn-face\" style=\"margin-top:8px\">Open with Face ID</button>"
    "<div class=\"status-line\" id=\"lock-line\"></div></div>"

    "<div class=\"card\"><h2>Environment</h2><div class=\"env-grid\">"
    "<div class=\"env-item\"><div class=\"v\" id=\"v-temp\">-</div><div class=\"l\">Temp</div></div>"
    "<div class=\"env-item\"><div class=\"v\" id=\"v-hum\">-</div><div class=\"l\">Humidity</div></div>"
    "<div class=\"env-item\"><div class=\"v\" id=\"v-press\">-</div><div class=\"l\">Pressure</div></div>"
    "<div class=\"env-item\"><div class=\"v\" id=\"v-pres\">-</div><div class=\"l\">Presence</div></div>"
    "<div class=\"env-item\"><div class=\"v\" id=\"v-lux\">-</div><div class=\"l\">Lux</div></div>"
    "</div></div>"

    "<div class=\"card\"><h2>ML Agent</h2>"
    "<div class=\"row\"><div class=\"dev\"><div class=\"icon\">&#129504;</div>"
    "<div><b>Learning mode</b><div class=\"small\" id=\"ml-hint\">Shadow - watch and learn</div></div></div>"
    "<button class=\"mode-pill\" id=\"ml-mode\">SHADOW</button></div>"
    "<div class=\"ml-rows\">"
    "<div class=\"row\"><span>Light should be</span><span id=\"ml-light\">-</span></div>"
    "<div class=\"row\"><span>Fan should be</span><span id=\"ml-fan\">-</span></div>"
    "<div class=\"row\"><span>Agreement (window)</span><span id=\"ml-acc\">-</span></div>"
    "</div></div>"

    "<div class=\"card\"><button class=\"btn secondary\" id=\"btn-settings\">System Settings</button></div>"

    // تنظیمات - پشت رمز سیستم (PASSWORD_KIND_SETTINGS روی سرور)
    "<div class=\"overlay\" id=\"settings\">"
    "<div class=\"row\"><h1 style=\"font-size:18px\">System Settings</h1>"
    "<button class=\"btn secondary\" id=\"btn-close-settings\" style=\"width:auto;padding:8px 14px\">Close</button></div>"

    "<div id=\"settings-login\" style=\"margin-top:20px\"><div class=\"card\">"
    "<h2>Restricted Area</h2>"
    "<p class=\"small\" style=\"margin-bottom:10px\">Enter the system settings password to continue.</p>"
    "<input type=\"password\" id=\"set-pin\" inputmode=\"numeric\" maxlength=\"8\" placeholder=\"Settings password\">"
    "<button class=\"btn\" id=\"btn-set-login\" style=\"margin-top:10px\">Unlock Settings</button>"
    "<div class=\"status-line\" id=\"set-line\"></div></div></div>"

    "<div id=\"settings-panel\" style=\"display:none\">"
    "<div class=\"card set-section\"><h2>MQTT Broker (Home Assistant)</h2>"
    "<input type=\"text\" id=\"mqtt-host\" placeholder=\"e.g. 192.168.1.50\" maxlength=\"64\">"
    "<button class=\"btn\" id=\"btn-mqtt-save\" style=\"margin-top:10px\">Save and Connect</button>"
    "<div class=\"status-line\" id=\"mqtt-line\"></div></div>"

    "<div class=\"card set-section\"><h2>Change Door Password</h2>"
    "<input type=\"password\" id=\"pw-lock-c\" inputmode=\"numeric\" placeholder=\"Current password\">"
    "<input type=\"password\" id=\"pw-lock-n\" inputmode=\"numeric\" placeholder=\"New password (4-8)\" style=\"margin-top:8px\">"
    "<input type=\"password\" id=\"pw-lock-cf\" inputmode=\"numeric\" placeholder=\"Repeat new password\" style=\"margin-top:8px\">"
    "<button class=\"btn\" id=\"btn-pw-lock\" style=\"margin-top:10px\">Change Door Password</button>"
    "<div class=\"status-line\" id=\"pw-lock-line\"></div></div>"

    "<div class=\"card set-section\"><h2>Change Settings Password</h2>"
    "<input type=\"password\" id=\"pw-set-c\" inputmode=\"numeric\" placeholder=\"Current password\">"
    "<input type=\"password\" id=\"pw-set-n\" inputmode=\"numeric\" placeholder=\"New password (4-8)\" style=\"margin-top:8px\">"
    "<input type=\"password\" id=\"pw-set-cf\" inputmode=\"numeric\" placeholder=\"Repeat new password\" style=\"margin-top:8px\">"
    "<button class=\"btn\" id=\"btn-pw-set\" style=\"margin-top:10px\">Change Settings Password</button>"
    "<div class=\"status-line\" id=\"pw-set-line\"></div></div>"

    "<div class=\"card set-section\"><h2>Enrolled Faces</h2>"
    "<div id=\"faces-list\"></div>"
    "<button class=\"btn\" id=\"btn-enroll\" style=\"margin-top:10px\">Add New Face</button>"
    "<div class=\"status-line\" id=\"enroll-line\"></div></div>"

    "<div class=\"card set-section\"><h2>System</h2>"
    "<p class=\"small\" id=\"sys-info\" style=\"margin-bottom:10px\"></p>"
    "<button class=\"btn danger\" id=\"btn-restart\">Restart Device</button></div>"
    "</div></div>"

    // مودال چهره - همان مسیر /api/face/recognize صفحه‌ی /recognize
    "<div class=\"overlay\" id=\"face-modal\">"
    "<div class=\"row\"><h1 style=\"font-size:18px\">Face ID</h1>"
    "<button class=\"btn secondary\" id=\"btn-face-close\" style=\"width:auto;padding:8px 14px\">Close</button></div>"
    "<video id=\"video\" autoplay playsinline></video>"
    "<canvas id=\"fc\" width=\"320\" height=\"240\" style=\"display:none\"></canvas>"
    "<div style=\"display:flex;gap:8px;margin-top:10px\">"
    "<button class=\"btn secondary\" id=\"btn-flip\" style=\"width:auto\">Flip</button>"
    "<button class=\"btn\" id=\"btn-scan\">Capture and Verify</button></div>"
    "<div id=\"face-status\" style=\"margin-top:12px;font-weight:700;min-height:22px\"></div></div>"

    "<div id=\"toast\"></div>"
    "<script>"
    "const $=function(id){return document.getElementById(id)};"
    "let st=null,faceStream=null,facing='user';"
    "function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');"
    "setTimeout(function(){t.classList.remove('show')},2600);}"
    "async function postForm(url,data){"
    "const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:new URLSearchParams(data).toString()});"
    "return {status:r.status,data:await r.json().catch(function(){return {}})}};"
    "function conf(p){return Math.round(Math.max(p,1-p)*100)}"
    "async function refresh(){"
    "try{"
    "const r=await fetch('/api/status',{cache:'no-store'});"
    "st=await r.json();"
    "$('dot').style.background='var(--green)';"
    "$('chip-ip').textContent=(st.wifi&&st.wifi.connected)?st.wifi.ip:'offline';"
    "$('chip-mqtt').textContent=st.mqtt?st.mqtt.state:'-';"
    "$('sw-light').checked=st.light;"
    "$('sw-fan').checked=st.fan;"
    "const lb=$('lock-state');"
    "lb.textContent=st.lock?'UNLOCKED':'LOCKED';"
    "lb.className='badge '+(st.lock?'unlocked':'locked');"
    "if(st.sensor&&st.sensor.valid){"
    "$('v-temp').textContent=st.sensor.temperature.toFixed(1);"
    "$('v-hum').textContent=st.sensor.humidity.toFixed(0);"
    "$('v-press').textContent=st.sensor.pressure.toFixed(0);"
    "}else{$('v-temp').textContent='-';$('v-hum').textContent='-';$('v-press').textContent='-';}"
    "$('v-pres').textContent=st.presence?'Home':'Away';"
    "$('v-lux').textContent=Math.round(st.lux)+' lx';"
    "$('ml-mode').textContent=st.ml.auto?'AUTO':'SHADOW';"
    "$('ml-mode').classList.toggle('auto',st.ml.auto);"
    "$('ml-hint').textContent=st.ml.auto?'Auto - acts on your behalf':'Shadow - watch and learn';"
    "$('ml-light').textContent=(st.ml.p_light>=0.5?'ON ':'OFF ')+conf(st.ml.p_light)+'%';"
    "$('ml-fan').textContent=(st.ml.p_fan>=0.5?'ON ':'OFF ')+conf(st.ml.p_fan)+'%';"
    "$('ml-acc').textContent=st.ml.n_light>0?"
    "'Light '+Math.round(st.ml.acc_light)+'% / Fan '+Math.round(st.ml.acc_fan)+'%':'-';"
    "}catch(e){$('dot').style.background='var(--red)';}"
    "}"
    "setInterval(refresh,2000);refresh();"
    "$('sw-light').addEventListener('change',function(e){"
    "postForm('/api/light/set',{on:e.target.checked?1:0}).then(refresh)});"
    "$('sw-fan').addEventListener('change',function(e){"
    "postForm('/api/fan/set',{on:e.target.checked?1:0}).then(refresh)});"
    "$('btn-unlock').addEventListener('click',async function(){"
    "const pin=$('lock-pin').value;"
    "if(!pin){toast('Enter the PIN first');return;}"
    "$('btn-unlock').disabled=true;$('lock-line').textContent='Verifying...';"
    "try{"
    "const res=await postForm('/api/lock/unlock',{password:pin});"
    "if(res.status===200&&res.data.ok){$('lock-line').textContent='Door unlocked';$('lock-pin').value='';}"
    "else{$('lock-line').textContent=res.data.error||'Wrong password';}"
    "}catch(e){$('lock-line').textContent='Error: '+e.message;}"
    "$('btn-unlock').disabled=false;refresh();});"
    "$('lock-pin').addEventListener('keydown',function(e){"
    "if(e.key==='Enter'){$('btn-unlock').click()}});"
    "$('ml-mode').addEventListener('click',async function(){"
    "if(!st||!st.ml){return;}"
    "const res=await postForm('/api/ml/mode',{auto:st.ml.auto?0:1});"
    "if(res.status!==200){toast('Failed to change ML mode');}"
    "refresh();});"
    // مودال چهره
    "async function startFaceCam(){"
    "if(faceStream){faceStream.getTracks().forEach(function(t){t.stop()});}"
    "try{"
    "faceStream=await navigator.mediaDevices.getUserMedia({video:{facingMode:facing}});"
    "$('video').srcObject=faceStream;$('video').style.display='block';"
    "}catch(e){$('face-status').textContent='Camera error: '+e.message;}"
    "}"
    "$('btn-face').addEventListener('click',function(){"
    "$('face-modal').classList.add('open');$('face-status').textContent='';startFaceCam();});"
    "$('btn-face-close').addEventListener('click',function(){"
    "$('face-modal').classList.remove('open');"
    "if(faceStream){faceStream.getTracks().forEach(function(t){t.stop()});faceStream=null;}});"
    "$('btn-flip').addEventListener('click',function(){"
    "facing=(facing==='user')?'environment':'user';startFaceCam();});"
    "$('btn-scan').addEventListener('click',function(){"
    "const c=$('fc'),ctx=c.getContext('2d'),v=$('video');"
    "if(!v.videoWidth){$('face-status').textContent='Camera not ready';return;}"
    "$('face-status').textContent='Verifying...';"
    "ctx.fillStyle='#000';ctx.fillRect(0,0,320,240);"
    "const s=Math.min(320/v.videoWidth,240/v.videoHeight);"
    "ctx.drawImage(v,(320-v.videoWidth*s)/2,(240-v.videoHeight*s)/2,v.videoWidth*s,v.videoHeight*s);"
    "c.toBlob(async function(blob){"
    "try{"
    "const r=await fetch('/api/face/recognize',{method:'POST',headers:{'Content-Type':'image/jpeg'},body:blob});"
    "const text=await r.text();"
    "$('face-status').textContent=(r.status===200?'OK: ':'')+text;"
    "if(r.status===200){refresh();}"
    "}catch(e){$('face-status').textContent='Error: '+e.message;}"
    "},'image/jpeg',0.85);});"
    // تنظیمات
    "function showLogin(){$('settings-login').style.display='block';$('settings-panel').style.display='none';}"
    "function renderFaces(faces){"
    "const el=$('faces-list');el.innerHTML='';"
    "if(!faces.length){el.innerHTML='<p class=\\\"small\\\">No faces enrolled yet</p>';return;}"
    "faces.forEach(function(f){"
    "const row=document.createElement('div');row.className='face-item';"
    "const nm=document.createElement('div');"
    "const b=document.createElement('b');b.textContent=f.name;"
    "const cnt=document.createElement('span');cnt.className='small';"
    "cnt.textContent=' ('+f.samples+' sample'+(f.samples==1?'':'s')+')';"
    "nm.appendChild(b);nm.appendChild(cnt);"
    "const del=document.createElement('button');del.className='btn danger';"
    "del.style.width='auto';del.style.padding='6px 12px';del.textContent='Delete';"
    "del.addEventListener('click',async function(){"
    "if(!confirm('Delete all samples of this face?')){return;}"
    "del.disabled=true;"
    "const res=await apiSettings({action:'delete_face',name:f.name});"
    "if(res.status===200&&res.data.ok){toast('Face deleted');row.remove();"
    "if(!el.children.length){el.innerHTML='<p class=\\\"small\\\">No faces enrolled yet</p>';}}"
    "else{toast(res.data.error||'Failed');del.disabled=false;}});"
    "row.appendChild(nm);row.appendChild(del);el.appendChild(row);});}"
    "function showPanel(d){"
    "$('settings-login').style.display='none';$('settings-panel').style.display='block';"
    "$('mqtt-host').value=d.mqtt_host||'';"
    "$('sys-info').textContent=(d.wifi_ssid?'WiFi: '+d.wifi_ssid+' / ':'')+'IP: '+d.ip;"
    "renderFaces(d.faces||[]);}"
    "async function apiSettings(data){"
    "const res=await postForm('/api/settings',data);"
    "if(res.status===401){showLogin();toast('Session expired - log in again');}"
    "return res;}"
    "$('btn-settings').addEventListener('click',function(){"
    "$('settings').classList.add('open');showLogin();});"
    "$('btn-close-settings').addEventListener('click',function(){"
    "$('settings').classList.remove('open');});"
    "$('btn-set-login').addEventListener('click',async function(){"
    "const pin=$('set-pin').value;if(!pin){return;}"
    "$('btn-set-login').disabled=true;$('set-line').textContent='Checking...';"
    "try{"
    "const res=await postForm('/api/settings/unlock',{password:pin});"
    "if(res.status===200&&res.data.ok){$('set-pin').value='';showPanel(res.data);}"
    "else{$('set-line').textContent=res.data.error||'Wrong password';}"
    "}catch(e){$('set-line').textContent='Error: '+e.message;}"
    "$('btn-set-login').disabled=false;});"
    "$('btn-mqtt-save').addEventListener('click',async function(){"
    "const host=$('mqtt-host').value.trim();"
    "if(!host){toast('Enter broker IP or hostname');return;}"
    "$('btn-mqtt-save').disabled=true;$('mqtt-line').textContent='Connecting... check the MQTT chip in the header';"
    "try{"
    "const res=await apiSettings({action:'mqtt',host:host});"
    "if(res.status!==200||!res.data.ok){$('mqtt-line').textContent=res.data.error||'Failed';}"
    "}catch(e){$('mqtt-line').textContent='Error: '+e.message;}"
    "$('btn-mqtt-save').disabled=false;});"
    "$('btn-enroll').addEventListener('click',async function(){"
    "$('btn-enroll').disabled=true;$('enroll-line').textContent='Preparing...';"
    "try{"
    "const res=await apiSettings({action:'enroll_link'});"
    "if(res.status===200&&res.data.ok){"
    "$('enroll-line').textContent='';"
    "window.location.href=res.data.url;"
    "return;"
    "}else{$('enroll-line').textContent=res.data.error||'Failed';}"
    "}catch(e){$('enroll-line').textContent='Error: '+e.message;}"
    "$('btn-enroll').disabled=false;});"
    "async function changePw(kind,c,n,cf,line){"
    "const cur=$(c).value,nw=$(n).value,cfv=$(cf).value,el=$(line);"
    "const alnum=/^[A-Za-z0-9]+$/;"
    "if(!alnum.test(nw)||nw.length<4||nw.length>8){el.textContent='New password must be 4-8 letters/digits';return;}"
    "if(nw!==cfv){el.textContent='New passwords do not match';return;}"
    "el.textContent='Saving...';"
    "try{"
    "const r=await fetch('/api/password',{method:'POST',"
    "headers:{'Content-Type':'application/x-www-form-urlencoded'},"
    "body:new URLSearchParams({type:kind,current:cur,new:nw,confirm:cfv}).toString()});"
    "const t=await r.text();"
    "el.textContent=(r.status===200?'OK: ':'')+t;"
    "if(r.status===200){$(c).value='';$(n).value='';$(cf).value='';}"
    "}catch(e){el.textContent='Error: '+e.message;}}"
    "$('btn-pw-lock').addEventListener('click',function(){"
    "changePw('lock','pw-lock-c','pw-lock-n','pw-lock-cf','pw-lock-line')});"
    "$('btn-pw-set').addEventListener('click',function(){"
    "changePw('settings','pw-set-c','pw-set-n','pw-set-cf','pw-set-line')});"
    "$('btn-restart').addEventListener('click',async function(){"
    "if(!confirm('Restart the device?')){return;}"
    "const res=await postForm('/api/settings/restart',{});"
    "toast(res.data.ok?'Restarting...':'Failed');});"
    "</script></body></html>";

// ---------------------------------------------------------------------
// هندلرهای کنترل دستگاه داشبورد
// ---------------------------------------------------------------------

static esp_err_t api_device_set_handler(httpd_req_t *req, bool fan)
{
    char body[64];
    char on_str[8] = {0};
    if (read_form_body(req, body, sizeof(body)) != ESP_OK ||
        !extract_form_value(body, "on", on_str, sizeof(on_str))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'on' field");
        return ESP_OK;
    }

    bool on = (strcmp(on_str, "1") == 0 || strcmp(on_str, "true") == 0);
    if (fan) {
        app_state_set_fan(on);
    } else {
        app_state_set_light(on);
    }

    char resp[48];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"%s\":%s}",
             fan ? "fan" : "light", on ? "true" : "false");
    return send_ok_json(req, resp);
}

static esp_err_t api_light_set_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/light/set");
    return api_device_set_handler(req, false);
}

static esp_err_t api_fan_set_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/fan/set");
    return api_device_set_handler(req, true);
}

// همان مسیر رسمی کیپد LCD: verify رمز قفل → app_state_set_lock (که خودش
// publish وضعیت و ری‌لاک خودکار ۵ ثانیه‌ای را انجام می‌دهد) + رویداد access
static esp_err_t api_lock_unlock_handler(httpd_req_t *req)
{
    char body[PASSWORD_FORM_MAX_SIZE + 1];
    char password[PASSWORD_MAX_LEN + 1] = {0};
    if (read_form_body(req, body, sizeof(body)) != ESP_OK ||
        !extract_form_value(body, "password", password, sizeof(password))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing password");
        return ESP_OK;
    }

    if (password_manager_verify(PASSWORD_KIND_LOCK, password)) {
        ESP_LOGI(TAG, "Door unlocked via dashboard password");
        app_state_set_lock(true);
        mqtt_manager_publish_access_event(ACCESS_EVENT_GRANTED_CODE);
        return send_ok_json(req, "{\"ok\":true}");
    }

    mqtt_manager_publish_access_event(ACCESS_EVENT_DENIED_CODE);
    ESP_LOGW(TAG, "Dashboard unlock rejected: wrong password");
    // محافظت سبک در برابر حدس آنلاین (ریسک brute-force شبکه‌ی محلی
    // همانند توکن Enroll آگاهانه پذیرفته شده؛ این فقط سرعت را کم می‌کند)
    vTaskDelay(pdMS_TO_TICKS(1000));
    httpd_resp_set_status(req, "403 Forbidden");
    return send_ok_json(req, "{\"ok\":false,\"error\":\"Wrong password\"}");
}

static esp_err_t api_ml_mode_handler(httpd_req_t *req)
{
    char body[64];
    char auto_str[8] = {0};
    if (read_form_body(req, body, sizeof(body)) != ESP_OK ||
        !extract_form_value(body, "auto", auto_str, sizeof(auto_str))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'auto' field");
        return ESP_OK;
    }

    bool auto_mode = (strcmp(auto_str, "1") == 0 || strcmp(auto_str, "true") == 0);
    ESP_LOGI(TAG, "ML autonomy via dashboard: %s", auto_mode ? "AUTO" : "SHADOW");
    ml_agent_set_autonomy(auto_mode);

    char resp[32];
    snprintf(resp, sizeof(resp), "{\"ok\":true,\"auto\":%s}", auto_mode ? "true" : "false");
    return send_ok_json(req, resp);
}

// ورود به تنظیمات: verify رمز سیستم → ساخت نشست (کوکی) + برگرداندن کل
// داده‌ی تنظیمات در یک پاسخ تا نیازی به GET جداگانه نباشد
static esp_err_t api_settings_unlock_handler(httpd_req_t *req)
{
    char body[PASSWORD_FORM_MAX_SIZE + 1];
    char password[PASSWORD_MAX_LEN + 1] = {0};
    if (read_form_body(req, body, sizeof(body)) != ESP_OK ||
        !extract_form_value(body, "password", password, sizeof(password))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing password");
        return ESP_OK;
    }

    if (!password_manager_verify(PASSWORD_KIND_SETTINGS, password)) {
        ESP_LOGW(TAG, "Dashboard settings login rejected");
        vTaskDelay(pdMS_TO_TICKS(1000));
        httpd_resp_set_status(req, "403 Forbidden");
        return send_ok_json(req, "{\"ok\":false,\"error\":\"Wrong settings password\"}");
    }

    settings_session_create();
    ESP_LOGI(TAG, "Dashboard settings session opened");

    char host_esc[2 * MQTT_CONFIG_HOST_MAX_LEN + 2];
    char ssid_esc[67];
    char mqtt_host[MQTT_CONFIG_HOST_MAX_LEN + 1];
    bool have_host = mqtt_config_get_host(mqtt_host, sizeof(mqtt_host));
    bool wifi_conn = wifi_is_connected();
    json_escape(have_host ? mqtt_host : "", host_esc, sizeof(host_esc));
    json_escape(wifi_conn ? wifi_get_connected_ssid() : "", ssid_esc, sizeof(ssid_esc));

    char faces[2048];
    size_t faces_len = faces_json(faces, sizeof(faces));

    snprintf(s_json_buf, sizeof(s_json_buf),
             "{\"ok\":true,\"mqtt_host\":\"%s\",\"mqtt_state\":\"%s\","
             "\"wifi_ssid\":\"%s\",\"ip\":\"%s\",\"faces\":%.*s}",
             host_esc, mqtt_state_str(mqtt_manager_get_state()),
             ssid_esc, wifi_conn ? wifi_get_ip_str() : "",
             (int)faces_len, faces);

    char cookie_hdr[96];
    snprintf(cookie_hdr, sizeof(cookie_hdr),
             "%s=%s; Path=/; Max-Age=600; HttpOnly; Secure; SameSite=Strict",
             SETTINGS_COOKIE_NAME, s_settings_session);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Set-Cookie", cookie_hdr);
    return httpd_resp_sendstr(req, s_json_buf);
}

// اتصال به بروکر جدید بلاک‌کننده است (~۵ ثانیه) - مثل صفحه‌ی تنظیمات MQTT
// روی LCD همیشه از تسک جداگانه اجرا می‌شود؛ نتیجه از طریق state در
// /api/status و چیپ MQTT دیده می‌شود
static void mqtt_save_web_task(void *arg)
{
    char *host = (char *)arg;
    bool ok = mqtt_manager_connect_and_save(host);
    ESP_LOGI(TAG, "Dashboard MQTT connect to %s: %s",
             host, ok ? "OK (saved)" : "FAILED (nothing saved)");
    free(host);
    vTaskDelete(NULL);
}

static bool is_valid_host_str(const char *s)
{
    if (s[0] == '\0') {
        return false;
    }
    for (const char *c = s; *c != '\0'; c++) {
        if (!isalnum((unsigned char)*c) && *c != '.' && *c != '-' && *c != ':') {
            return false;
        }
    }
    return true;
}

// POST /api/settings - اکشن‌های پشت نشست: تغییر بروکر، حذف چهره،
// ساخت لینک Enroll، لیست تازه‌ی چهره‌ها
static esp_err_t api_settings_handler(httpd_req_t *req)
{
    if (!settings_session_valid(req)) {
        send_unauthorized(req);
        return ESP_OK;
    }

    char body[PASSWORD_FORM_MAX_SIZE + 1];
    char action[24] = {0};
    if (read_form_body(req, body, sizeof(body)) != ESP_OK ||
        !extract_form_value(body, "action", action, sizeof(action))) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing action");
        return ESP_OK;
    }

    if (strcmp(action, "mqtt") == 0) {
        char host[MQTT_CONFIG_HOST_MAX_LEN + 1] = {0};
        if (!extract_form_value(body, "host", host, sizeof(host)) ||
            !is_valid_host_str(host)) {
            httpd_resp_set_status(req, "400 Bad Request");
            return send_ok_json(req, "{\"ok\":false,\"error\":\"Invalid host\"}");
        }

        char *host_copy = malloc(strlen(host) + 1);
        if (host_copy == NULL) {
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Out of memory");
            return ESP_OK;
        }
        strcpy(host_copy, host);

        if (xTaskCreate(mqtt_save_web_task, "mqtt_web", 4096, host_copy, 3, NULL) != pdPASS) {
            free(host_copy);
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start connect task");
            return ESP_OK;
        }
        return send_ok_json(req, "{\"ok\":true,\"connecting\":true}");
    }

    if (strcmp(action, "delete_face") == 0) {
        char name[FACE_DB_NAME_MAX_LEN + 1] = {0};
        if (!extract_form_value(body, "name", name, sizeof(name)) || name[0] == '\0') {
            httpd_resp_set_status(req, "400 Bad Request");
            return send_ok_json(req, "{\"ok\":false,\"error\":\"Missing name\"}");
        }

        face_work_item_t item = { .req = NULL, .op = FACE_OP_DELETE };
        strncpy(item.name, name, FACE_DB_NAME_MAX_LEN);
        item.name[FACE_DB_NAME_MAX_LEN] = '\0';

        if (xQueueSend(s_face_queue, &item, 0) != pdTRUE) {
            httpd_resp_set_status(req, "503 Server Busy");
            return send_ok_json(req, "{\"ok\":false,\"error\":\"Busy, try again\"}");
        }
        ESP_LOGI(TAG, "Face delete queued: \"%s\"", item.name);
        return send_ok_json(req, "{\"ok\":true}");
    }

    if (strcmp(action, "enroll_link") == 0) {
        if (!wifi_is_connected()) {
            httpd_resp_set_status(req, "400 Bad Request");
            return send_ok_json(req, "{\"ok\":false,\"error\":\"WiFi not connected\"}");
        }
        char token[ENROLL_TOKEN_LEN + 1];
        enroll_token_generate(token, sizeof(token));
        char url[64];
        snprintf(url, sizeof(url), "https://%s/enroll?token=%s", wifi_get_ip_str(), token);
        char resp[128];
        snprintf(resp, sizeof(resp), "{\"ok\":true,\"url\":\"%s\"}", url);
        return send_ok_json(req, resp);
    }

    if (strcmp(action, "list_faces") == 0) {
        char faces[2048];
        size_t faces_len = faces_json(faces, sizeof(faces));
        snprintf(s_json_buf, sizeof(s_json_buf),
                 "{\"ok\":true,\"faces\":%.*s}", (int)faces_len, faces);
        return send_ok_json(req, s_json_buf);
    }

    httpd_resp_set_status(req, "400 Bad Request");
    return send_ok_json(req, "{\"ok\":false,\"error\":\"Unknown action\"}");
}

static esp_err_t api_settings_restart_handler(httpd_req_t *req)
{
    if (!settings_session_valid(req)) {
        send_unauthorized(req);
        return ESP_OK;
    }

    ESP_LOGW(TAG, "Device restart requested via dashboard settings");
    send_ok_json(req, "{\"ok\":true,\"message\":\"Restarting...\"}");
    vTaskDelay(pdMS_TO_TICKS(500));   // فرصت رسیدن پاسخ قبل از ری‌استارت
    esp_restart();
    return ESP_OK;   // دست‌یافتنی
}

// ---------------------------------------------------------------------
// پردازش سنگین چهره - این تابع فقط داخل face_worker_task اجرا می‌شود
// ---------------------------------------------------------------------

// حداکثر زمان کل برای دریافت بدنه: قبلاً روی timeout هر recv بی‌قید continue
// می‌شد، پس کلاینتی که وسط آپلود میخوابید face_worker را تا ابد بلاک می‌کرد
#define FACE_BODY_DEADLINE_MS   20000
// فاصله‌ی حداکثر تغذیه‌ی TWDT توسط face_worker - باید از TASK_WDT_TIMEOUT_S کوچک‌تر باشد
#define FACE_WDT_FEED_MS        5000

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

    int64_t deadline_us = esp_timer_get_time() + ((int64_t)FACE_BODY_DEADLINE_MS * 1000);
    size_t total_received = 0;
    while (total_received < content_len) {
        int received = httpd_req_recv(req, (char *)buf + total_received, content_len - total_received);
        if (received == HTTPD_SOCK_ERR_TIMEOUT) {
            if (esp_timer_get_time() > deadline_us) {
                ESP_LOGE(TAG, "Client stalled mid-upload (%u/%u bytes) - aborting",
                         (unsigned)total_received, (unsigned)content_len);
                heap_caps_free(buf);
                return ESP_ERR_TIMEOUT;
            }
            continue;   // مهلت کل هنوز تمام نشده - منتظر بایت بعدی
        }
        if (received <= 0) {
            ESP_LOGE(TAG, "Receive failed after %u/%u bytes",
                     (unsigned)total_received, (unsigned)content_len);
            heap_caps_free(buf);
            return ESP_FAIL;
        }
        total_received += received;
        esp_task_wdt_reset();   // آپلودهای کند شبکه نباید TWDT را بنشانند
    }

    // امضای JPEG (SOI) - زودتر از کتابخانه رد می‌شود تا خطای شفاف به کلاینت برسد
    if (total_received < 2 || buf[0] != 0xFF || buf[1] != 0xD8) {
        heap_caps_free(buf);
        return ESP_ERR_INVALID_RESPONSE;
    }

    *out_buf = buf;
    *out_len = total_received;
    return ESP_OK;
}

// پاسخ HTTP مناسب برای هر کد خطای receive_jpeg_body - مشترک بین recognize و enroll
static void send_receive_body_error(httpd_req_t *req, esp_err_t ret)
{
    switch (ret) {
    case ESP_ERR_INVALID_SIZE:
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid image size");
        break;
    case ESP_ERR_INVALID_RESPONSE:
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Not a JPEG image");
        break;
    case ESP_ERR_TIMEOUT:
        httpd_resp_set_status(req, "408 Request Timeout");
        httpd_resp_sendstr(req, "Client stalled during image upload");
        break;
    default:
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive image");
        break;
    }
}

static void handle_recognize(httpd_req_t *req)
{
    uint8_t *jpeg_buf = NULL;
    size_t len = 0;

    esp_err_t ret = receive_jpeg_body(req, &jpeg_buf, &len);
    if (ret != ESP_OK) {
        send_receive_body_error(req, ret);
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
        char name[FACE_DB_NAME_MAX_LEN + 1];
        char resp[96];
        if (face_db_get_name((uint16_t)detected_id, name, sizeof(name))) {
            snprintf(resp, sizeof(resp), "Access Granted! Welcome, %s", name);
        } else {
            snprintf(resp, sizeof(resp), "Access Granted! Welcome User ID: %d", detected_id);
        }
        httpd_resp_sendstr(req, resp);
        app_state_set_lock(true);
        mqtt_manager_publish_access_event(ACCESS_EVENT_GRANTED_FACE);
    } else {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Access Denied: Unknown Face");
        mqtt_manager_publish_access_event(ACCESS_EVENT_DENIED_FACE); 

    }
}

static void handle_enroll(httpd_req_t *req, const char *name)
{
    uint8_t *jpeg_buf = NULL;
    size_t len = 0;

    esp_err_t ret = receive_jpeg_body(req, &jpeg_buf, &len);
    if (ret != ESP_OK) {
        send_receive_body_error(req, ret);
        return;
    }

    int new_id = 0;
    ret = face_recognition_enroll(jpeg_buf, len, &new_id);
    heap_caps_free(jpeg_buf);

    if (ret != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Enrollment failed");
        return;
    }

    esp_err_t db_ret = face_db_add((uint16_t)new_id, name);
    if (db_ret != ESP_OK) {
        // نمونه‌ی بدون نگاشت نام در face_db یتیم می‌شود: نه در لیست دیده و نه
        // حذف می‌شود؛ پس feature تازه‌ساخته را پس می‌گیریم و خطا برمی‌گردانیم
        face_recognition_delete((uint16_t)new_id);
        ESP_LOGE(TAG, "Enrollment rolled back (ID %d): failed to save name: %s",
                 new_id, esp_err_to_name(db_ret));
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to save enrollment");
        return;
    }

    size_t sample_no = face_db_count_by_name(name);
    char resp[96];
    snprintf(resp, sizeof(resp), "Sample %u enrolled for %s (ID: %d)",
             (unsigned)sample_no, name, new_id);
    httpd_resp_sendstr(req, resp);
}

// ---------------------------------------------------------------------
// Task اختصاصی: تنها مصرف‌کننده‌ی صف کار
// ---------------------------------------------------------------------

// فقط داخل face_worker_task - همه‌ی نمونه‌های شخص از face_db و feature های
// متناظر از کتابخانه‌ی تشخیص حذف می‌شوند (هم‌الگوی صفحه‌ی تنظیمات LCD)
static void handle_delete_face(const char *name)
{
    uint16_t removed_ids[FACE_DB_MAX_ENTRIES];
    size_t removed = 0;
    if (face_db_remove_by_name(name, removed_ids, FACE_DB_MAX_ENTRIES, &removed) == ESP_OK) {
        for (size_t i = 0; i < removed; i++) {
            face_recognition_delete(removed_ids[i]);
        }
        ESP_LOGI(TAG, "Deleted %u sample(s) of \"%s\" via dashboard", (unsigned)removed, name);
    } else {
        ESP_LOGW(TAG, "Dashboard face delete: \"%s\" not found", name);
    }
}

static void face_worker_task(void *arg)
{
    face_work_item_t item;

    // عضویت در Task Watchdog: اگر هر چیزی این تسک را گیر اندازد، TWDT آن را
    // گزارش می‌کند (و با CONFIG_ESP_TASK_WDT_PANIC دستگاه را ریبوت می‌کند).
    // نکته: تسک در بیکاری روی صف می‌ماند، پس منتظرِ صف timeout دارد و در انتهای
    // هر دور حلقه (بیکار یا پس از پردازش) به TWDT خبر می‌دهد؛ فقط گیر کردن
    // واقعی که reset نزند باعث panic می‌شود. قبلاً بلاکِ portMAX_DELAY روی صف
    // خالی، هر ۱۵ ثانیه panic/ریبوت می‌ساخت.
    esp_task_wdt_add(NULL);

    while (1) {
        if (xQueueReceive(s_face_queue, &item, pdMS_TO_TICKS(FACE_WDT_FEED_MS)) == pdTRUE) {
            ESP_LOGI(TAG, "Face worker processing %s request",
                     item.op == FACE_OP_RECOGNIZE ? "recognize" :
                     item.op == FACE_OP_ENROLL    ? "enroll" : "delete");

            if (item.op == FACE_OP_RECOGNIZE) {
                handle_recognize(item.req);
            } else if (item.op == FACE_OP_ENROLL) {
                handle_enroll(item.req, item.name);
            } else {
                handle_delete_face(item.name);
            }

            // حذف چهره درخواست HTTP ای ندارد که کامل شود
            if (item.req != NULL) {
                httpd_req_async_handler_complete(item.req);
            }
        }

        esp_task_wdt_reset();
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
    char query[128] = {0};
    char token[ENROLL_TOKEN_LEN + 1] = {0};
    char name_raw[FACE_DB_NAME_MAX_LEN * 3 + 1] = {0};   // فضای اضافه برای %XX encoding
    char name[FACE_DB_NAME_MAX_LEN + 1] = {0};

    if (httpd_req_get_url_query_len(req) > 0 &&
        httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "token", token, sizeof(token));
        httpd_query_key_value(query, "name", name_raw, sizeof(name_raw));
    }
    url_decode(name, name_raw, sizeof(name));

    if (!enroll_token_validate(token)) {
        ESP_LOGW(TAG, "Enroll rejected: invalid or missing token");
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_sendstr(req, "Invalid or expired enrollment token");
        return ESP_OK;
    }

    if (name[0] == '\0') {
        ESP_LOGW(TAG, "Enroll rejected: missing name");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_sendstr(req, "Name is required");
        return ESP_OK;
    }

    httpd_req_t *copy = NULL;
    esp_err_t err = httpd_req_async_handler_begin(req, &copy);
    if (err != ESP_OK) {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start async request");
        return ESP_FAIL;
    }

    face_work_item_t item = { .req = copy, .op = FACE_OP_ENROLL };
    strncpy(item.name, name, FACE_DB_NAME_MAX_LEN);
    item.name[FACE_DB_NAME_MAX_LEN] = '\0';

    if (xQueueSend(s_face_queue, &item, 0) != pdTRUE) {
        httpd_resp_set_status(copy, "503 Server Busy");
        httpd_resp_sendstr(copy, "Face recognition is busy processing another request, try again shortly");
        httpd_req_async_handler_complete(copy);
        return ESP_OK;
    }

    ESP_LOGI(TAG, "POST /api/face/enroll received (token valid, name=\"%s\", enqueueing)", name);
    return ESP_OK;
}

static esp_err_t recognize_page_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, recognize_html, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t enroll_page_handler(httpd_req_t *req)
{
    char query[64] = {0};
    char token[ENROLL_TOKEN_LEN + 1] = {0};

    if (httpd_req_get_url_query_len(req) > 0 &&
        httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        httpd_query_key_value(query, "token", token, sizeof(token));
    }

    if (!enroll_token_validate(token)) {
        httpd_resp_set_status(req, "403 Forbidden");
        httpd_resp_set_type(req, "text/html; charset=utf-8");
        httpd_resp_sendstr(req,
            "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">"
            "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
            "<title>Enrollment</title></head>"
            "<body style=\"font-family:-apple-system,'Segoe UI',Roboto,sans-serif;"
            "background:#111418;color:#e8eaed;text-align:center;margin-top:80px;padding:0 20px\">"
            "<h2>Link expired or invalid</h2>"
            "<p style=\"color:#8b95a1;margin-top:10px\">Enrollment links are valid for 3 minutes."
            " Open the dashboard, go to System Settings and tap Add New Face again.</p>"
            "<p style=\"margin-top:24px\"><a href=\"/\" style=\"color:#4aa3ff\">Open Dashboard</a></p>"
            "</body></html>");
        return ESP_OK;
    }

    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, enroll_html, HTTPD_RESP_USE_STRLEN);
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
static const httpd_uri_t password_page_uri = { .uri = "/password", .method = HTTP_GET, .handler = password_page_handler, .user_ctx = NULL };
static const httpd_uri_t api_password_uri = { .uri = "/api/password", .method = HTTP_POST, .handler = api_password_handler, .user_ctx = NULL };
static const httpd_uri_t recognize_page_uri = { .uri = "/recognize", .method = HTTP_GET, .handler = recognize_page_handler, .user_ctx = NULL };
static const httpd_uri_t enroll_page_uri    = { .uri = "/enroll",    .method = HTTP_GET, .handler = enroll_page_handler,    .user_ctx = NULL };
// داشبورد وب
static const httpd_uri_t api_light_set_uri      = { .uri = "/api/light/set",       .method = HTTP_POST, .handler = api_light_set_handler,      .user_ctx = NULL };
static const httpd_uri_t api_fan_set_uri        = { .uri = "/api/fan/set",         .method = HTTP_POST, .handler = api_fan_set_handler,        .user_ctx = NULL };
static const httpd_uri_t api_lock_unlock_uri    = { .uri = "/api/lock/unlock",     .method = HTTP_POST, .handler = api_lock_unlock_handler,    .user_ctx = NULL };
static const httpd_uri_t api_ml_mode_uri        = { .uri = "/api/ml/mode",         .method = HTTP_POST, .handler = api_ml_mode_handler,        .user_ctx = NULL };
static const httpd_uri_t api_settings_unlock_uri = { .uri = "/api/settings/unlock", .method = HTTP_POST, .handler = api_settings_unlock_handler, .user_ctx = NULL };
static const httpd_uri_t api_settings_uri       = { .uri = "/api/settings",        .method = HTTP_POST, .handler = api_settings_handler,       .user_ctx = NULL };
static const httpd_uri_t api_settings_restart_uri = { .uri = "/api/settings/restart", .method = HTTP_POST, .handler = api_settings_restart_handler, .user_ctx = NULL };



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
    config.httpd.max_uri_handlers  = 18;

    // داشبورد وب مدام poll می‌کند؛ بدون این دو، هر درخواست یک handshake
    // تازه‌ی TLS می‌خواهد و بعد از چند دقیقه حافظه‌ی داخلی وسط handshake
    // تمام می‌شود (MBEDTLS_ERR_SSL_ALLOC_FAILED -0x7780) و سرور دیگر
    // اتصال جدید نمی‌پذیرد. TCP keep-alive اتصال‌های مرده (گوشی خوابیده)
    // را پس از ~۴۵ ثانیه آزاد می‌کند و lru_purge وقتی جا نیست قدیمی‌ترین
    // اتصال بلااستفاده را بازیافت می‌کند.
    config.httpd.keep_alive_enable = true;
    config.httpd.keep_alive_idle   = 30;
    config.httpd.keep_alive_interval = 5;
    config.httpd.keep_alive_count  = 3;
    config.httpd.lru_purge_enable  = true;

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
        { &recognize_page_uri,   "/recognize" },
        { &enroll_page_uri,      "/enroll" },
        { &password_page_uri,    "/password" },
        { &api_password_uri,     "/api/password" },
        { &api_light_set_uri,    "/api/light/set" },
        { &api_fan_set_uri,      "/api/fan/set" },
        { &api_lock_unlock_uri,  "/api/lock/unlock" },
        { &api_ml_mode_uri,      "/api/ml/mode" },
        { &api_settings_unlock_uri, "/api/settings/unlock" },
        { &api_settings_uri,     "/api/settings" },
        { &api_settings_restart_uri, "/api/settings/restart" },
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