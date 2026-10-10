/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "FreeRTOS.h"
static inline void vTaskDelay(unsigned n){(void)n;}
static inline void vTaskDelete(void *p){(void)p;}
static inline unsigned ulTaskNotifyTake(int b,unsigned n){(void)b;(void)n;return 1;}
static inline void xTaskNotifyGive(void *p){(void)p;}
static inline int xTaskCreatePinnedToCore(void(*fn)(void*),const char *name,int stack,void *p,int priority,void **out,int core){(void)fn;(void)name;(void)stack;(void)p;(void)priority;(void)out;(void)core;return 0;}

