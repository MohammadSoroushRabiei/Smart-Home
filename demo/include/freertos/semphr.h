/**
 * @file semphr.h
 * @brief شیم PC برای سمافورهای FreeRTOS - mutex های pthread.
 */
#pragma once

#include "FreeRTOS.h"
#include <pthread.h>

typedef pthread_mutex_t *SemaphoreHandle_t;

SemaphoreHandle_t xSemaphoreCreateMutex(void);
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void);
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, unsigned ms);
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem);
void vSemaphoreDelete(SemaphoreHandle_t sem);
