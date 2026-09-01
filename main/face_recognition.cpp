#include "face_recognition.h"

#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "jpeg_decoder.h" // هدر کامپوننت esp_jpeg
#include <string>

static const char *TAG = "FACE_REC";

static HumanFaceDetect *face_detector = nullptr;
static HumanFaceRecognizer *face_recognizer = nullptr;

extern "C" esp_err_t face_recognition_init(void)
{
    // ۱. Mount کردن پارتیشن SPIFFS
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = "storage",
        .max_files = 5,
        .format_if_mount_failed = true
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount or format SPIFFS: %s", esp_err_to_name(ret));
        return ret;
    }

    // ۲. مقداردهی اولیه مدل‌ها
    if (!face_detector) {
        face_detector = new HumanFaceDetect();
    }
    
    if (!face_recognizer) {
        std::string db_path = "/spiffs/face_db";
        face_recognizer = new HumanFaceRecognizer(db_path);
    }

    ESP_LOGI(TAG, "Face recognition models initialized successfully");
    return ESP_OK;
}

static esp_err_t process_image_internal(const uint8_t *jpeg_data, size_t len, bool is_enroll, bool *is_unlocked, int *out_id)
{
    if (len == 0 || len > 300 * 1024) {
        ESP_LOGE(TAG, "Invalid image size: %zu bytes", len);
        return ESP_ERR_INVALID_SIZE;
    }

    ESP_LOGI(TAG, "Received JPEG size: %zu bytes", len);
    if (len >= 2) {
        ESP_LOGI(TAG, "JPEG Header check: 0x%02X 0x%02X (Expected: 0xFF 0xD8)", jpeg_data[0], jpeg_data[1]);
    }

    // تخصیص بافر خروجی در PSRAM (حداکثر برای ۳۲۰x۲۴۰ با فرمت RGB565 که ۲ بایت per پیکسل است = ۱۵۳,۶۰۰ بایت)
    size_t max_out_size = 320 * 240 * 2; 
    uint8_t *out_buf = (uint8_t *)heap_caps_malloc(max_out_size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!out_buf) {
        ESP_LOGE(TAG, "Failed to allocate output buffer in PSRAM");
        return ESP_ERR_NO_MEM;
    }


    esp_jpeg_image_cfg_t jpeg_cfg = {}; 
    
    jpeg_cfg.indata = (uint8_t *)jpeg_data;
    jpeg_cfg.indata_size = len;
    jpeg_cfg.outbuf = out_buf;
    jpeg_cfg.outbuf_size = max_out_size;
    jpeg_cfg.out_format = JPEG_IMAGE_FORMAT_RGB565;
    jpeg_cfg.out_scale = JPEG_IMAGE_SCALE_0;
    jpeg_cfg.flags.swap_color_bytes = 0; 

    esp_jpeg_image_output_t output_img = {};
    
    // اجرای عملیات Decode با شتاب‌دهنده سخت‌افزاری
    esp_err_t ret = esp_jpeg_decode(&jpeg_cfg, &output_img);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_jpeg_decode failed: %s", esp_err_to_name(ret));
        heap_caps_free(out_buf);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Decoded successfully! Width: %d, Height: %d, Output size: %zu", 
             output_img.width, output_img.height, output_img.output_len);

    // ساختار تصویر برای کتابخانه esp-dl
    dl::image::img_t img;
    img.data = out_buf;
    img.width = output_img.width;
    img.height = output_img.height;
    img.pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565LE;

    // ۲. تشخیص چهره (Face Detection)
    auto detect_results = face_detector->run(img);
    if (detect_results.empty()) {
        ESP_LOGW(TAG, "No face detected in the image");
        heap_caps_free(out_buf);
        return ESP_ERR_NOT_FOUND;
    }

    // ۳. عملیات شناسایی یا ثبت
    if (is_enroll) {
        face_recognizer->enroll(img, detect_results);
        *out_id = 1; // در این PoC شناسه را به صورت ساده ۱ در نظر می‌گیریم
        ESP_LOGI(TAG, "Face enrolled successfully");
    } else {
        auto rec_results = face_recognizer->recognize(img, detect_results);
        if (!rec_results.empty() && rec_results[0].similarity > 0.70f) {
            *is_unlocked = true;
            *out_id = rec_results[0].id;
            ESP_LOGI(TAG, "Face recognized! ID: %d, Similarity: %.2f", *out_id, rec_results[0].similarity);
        } else {
            *is_unlocked = false;
            ESP_LOGW(TAG, "Unknown face. Highest similarity: %.2f", rec_results.empty() ? 0.0f : rec_results[0].similarity);
        }
    }

    // ۴. آزادسازی بافر تخصیص داده شده
    heap_caps_free(out_buf);
    return ESP_OK;
}

extern "C" esp_err_t face_recognition_process(const uint8_t *jpeg_data, size_t len, bool *is_unlocked, int *detected_id)
{
    return process_image_internal(jpeg_data, len, false, is_unlocked, detected_id);
}

extern "C" esp_err_t face_recognition_enroll(const uint8_t *jpeg_data, size_t len, int *new_id)
{
    return process_image_internal(jpeg_data, len, true, nullptr, new_id);
}