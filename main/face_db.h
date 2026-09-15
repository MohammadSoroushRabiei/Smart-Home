#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FACE_DB_NAME_MAX_LEN   23
#define FACE_DB_MAX_ENTRIES    20

typedef struct {
    uint16_t id;
    char name[FACE_DB_NAME_MAX_LEN + 1];
} face_db_entry_t;

/**
 * @brief راه‌اندازی اولیه: بارگذاری لیست id/نام از NVS به حافظه.
 *        باید یک‌بار در ابتدای app_main صدا زده شود.
 */
esp_err_t face_db_init(void);

/**
 * @brief افزودن یک رکورد جدید (بعد از enroll موفق در کتابخانه‌ی تشخیص چهره).
 */
esp_err_t face_db_add(uint16_t id, const char *name);

/**
 * @brief حذف یک رکورد بر اساس id (بعد از face_recognition_delete موفق).
 */
esp_err_t face_db_remove(uint16_t id);

/**
 * @brief کپی کردن تا max_count رکورد در out_array؛ تعداد واقعی کپی‌شده را برمی‌گرداند.
 */
size_t face_db_get_list(face_db_entry_t *out_array, size_t max_count);

/**
 * @brief جستجوی نام بر اساس id؛ اگر پیدا نشد false برمی‌گرداند.
 */
bool face_db_get_name(uint16_t id, char *out_buf, size_t out_size);

#ifdef __cplusplus
}
#endif