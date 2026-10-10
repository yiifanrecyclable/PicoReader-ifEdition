/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdint.h>
#define portMAX_DELAY 0xffffffff
#define pdPASS 1
#define pdTRUE 1
#define pdMS_TO_TICKS(n) (n)
typedef unsigned TickType_t;
typedef void* TaskHandle_t;
typedef void* SemaphoreHandle_t;

