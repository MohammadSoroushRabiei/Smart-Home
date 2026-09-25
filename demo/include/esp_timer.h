/**
 * @file esp_timer.h
 * @brief شیم PC برای esp_timer - ساعت مونوتونیک + تایمرهای نرم‌افزاری روی
 *        یک تیک‌تر (پیاده‌سازی در demo/src/shims.c). app_state (ری‌لاک) و
 *        lock (fail-safe) از همین تایمرها استفاده می‌کنند.
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef void *esp_timer_handle_t;

typedef struct {
    void (*callback)(void *arg);
    void *arg;
    int dispatch_method;          // روی PC بی‌استفاده
    const char *name;
    bool skip_unhandled_events;   // روی PC بی‌استفاده
} esp_timer_create_args_t;

int64_t esp_timer_get_time(void);

esp_err_t esp_timer_create(const esp_timer_create_args_t *args,
                           esp_timer_handle_t *out);
esp_err_t esp_timer_start_once(esp_timer_handle_t timer, uint64_t us);
esp_err_t esp_timer_start_periodic(esp_timer_handle_t timer, uint64_t us);
esp_err_t esp_timer_stop(esp_timer_handle_t timer);
esp_err_t esp_timer_delete(esp_timer_handle_t timer);
