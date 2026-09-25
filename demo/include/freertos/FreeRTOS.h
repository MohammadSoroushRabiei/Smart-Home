/**
 * @file FreeRTOS.h
 * @brief شیم PC برای تیپ‌ها و ماکروهای FreeRTOS. tick = 1ms.
 */
#pragma once

#include <stdint.h>

typedef long BaseType_t;
typedef unsigned long UBaseType_t;
typedef uint32_t TickType_t;

#define pdTRUE  1
#define pdFALSE 0
#define pdPASS  1
#define pdFAIL  0

#define pdMS_TO_TICKS(ms) ((TickType_t)(ms))
#define portMAX_DELAY     ((TickType_t)0xFFFFFFFFu)
#define portTICK_PERIOD_MS 1u
