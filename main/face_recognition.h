#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief راه‌اندازی اولیه ماژول تشخیص چهره (Mount کردن SPIFFS و لود مدل‌ها)
 */
esp_err_t face_recognition_init(void);

/**
 * @brief پردازش تصویر JPEG دریافتی و بررسی هویت چهره
 * 
 * @param jpeg_data اشاره‌گر به باینری تصویر JPEG
 * @param len حجم بایت‌های تصویر
 * @param is_unlocked خروجی: آیا چهره شناسایی و تطابق داشت؟
 * @param detected_id خروجی: شناسه چهره شناسایی شده (در صورت موفقیت)
 * @return esp_err_t 
 */
esp_err_t face_recognition_process(const uint8_t *jpeg_data, size_t len, bool *is_unlocked, int *detected_id);

/**
 * @brief ثبت (Enroll) یک چهره جدید در دیتابیس
 * 
 * @param jpeg_data اشاره‌گر به باینری تصویر JPEG
 * @param len حجم بایت‌های تصویر
 * @param new_id خروجی: شناسه اختصاص داده شده به چهره جدید
 * @return esp_err_t 
 */
esp_err_t face_recognition_enroll(const uint8_t *jpeg_data, size_t len, int *new_id);

#ifdef __cplusplus
}
#endif