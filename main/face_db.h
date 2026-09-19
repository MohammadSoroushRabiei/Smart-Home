#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FACE_DB_NAME_MAX_LEN   23
// ظرفیت بر حسب «نمونه» است نه شخص؛ با enroll چند-نمونه‌ای هر شخص چند ردیف
// (هر کدام یک feature مستقل در تشخیص چهره) با نام مشترک اشغال می‌کند
#define FACE_DB_MAX_ENTRIES    60

typedef struct {
    uint16_t id;
    char name[FACE_DB_NAME_MAX_LEN + 1];
} face_db_entry_t;

typedef struct {
    char name[FACE_DB_NAME_MAX_LEN + 1];
    size_t sample_count;
} face_db_person_t;

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

/**
 * @brief تعداد نمونه‌های ثبت‌شده برای یک نام.
 */
size_t face_db_count_by_name(const char *name);

/**
 * @brief لیست تجمیع‌شده‌ی افراد (نام‌های یکتا + تعداد نمونه‌ی هرکدام)،
 *        به ترتیب اولین ثبت. تعداد واقعی افراد را برمی‌گرداند.
 */
size_t face_db_get_persons(face_db_person_t *out_array, size_t max_count);

/**
 * @brief حذف همه‌ی نمونه‌های یک شخص بر اساس نام. شناسه‌های حذف‌شده در
 *        removed_ids کپی می‌شوند (تا max_ids تا) تا فراخوان بتواند feature
 *        متناظر را هم از کتابخانه‌ی تشخیص چهره پاک کند.
 *
 * @return ESP_OK اگر حداقل یک نمونه حذف شد؛ ESP_ERR_NOT_FOUND اگر نامی نبود
 */
esp_err_t face_db_remove_by_name(const char *name, uint16_t *removed_ids,
                                 size_t max_ids, size_t *removed_count);

#ifdef __cplusplus
}
#endif