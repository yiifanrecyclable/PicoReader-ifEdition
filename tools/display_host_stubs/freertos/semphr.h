#pragma once
#include <assert.h>
typedef struct {int depth;} StaticSemaphore_t;
typedef StaticSemaphore_t *SemaphoreHandle_t;
static inline SemaphoreHandle_t xSemaphoreCreateRecursiveMutexStatic(StaticSemaphore_t *s){return s;}
static inline void xSemaphoreTakeRecursive(SemaphoreHandle_t s,int t){(void)t;++s->depth;}
static inline void xSemaphoreGiveRecursive(SemaphoreHandle_t s){assert(s->depth>0);--s->depth;}
