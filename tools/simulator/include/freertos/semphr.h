/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include "FreeRTOS.h"
static inline int xSemaphoreTake(void *p,unsigned n){(void)p;(void)n;return 1;}
static inline int xSemaphoreGive(void *p){(void)p;return 1;}
static inline void *xSemaphoreCreateMutex(void){return (void*)1;}
static inline void *xSemaphoreCreateBinary(void){return (void*)1;}
static inline void vSemaphoreDelete(void *p){(void)p;}

