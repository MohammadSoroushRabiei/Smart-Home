/**
 * @file task.h
 * @brief شیم PC برای تسک‌های FreeRTOS - روی pthread سوار می‌شود.
 */
#pragma once

#include "FreeRTOS.h"

typedef void (*TaskFunction_t)(void *);

int xTaskCreate(TaskFunction_t fn, const char *name, unsigned stack_depth,
                void *arg, unsigned priority, void **handle);

int xTaskCreatePinnedToCore(TaskFunction_t fn, const char *name,
                            unsigned stack_depth, void *arg, unsigned priority,
                            void **handle, int core);

void vTaskDelay(unsigned ms);
void vTaskDelete(void *handle);
