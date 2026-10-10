/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
static inline size_t heap_caps_get_free_size(int caps){(void)caps;return 8u*1024u*1024u;}
// 宿主堆不模拟硬件内存容量。/ Host heap does not emulate device memory capacity.
static inline void *heap_caps_malloc(size_t n, int caps) { (void)caps; return malloc(n); }
static inline void *heap_caps_calloc(size_t n, size_t size, int caps) { (void)caps; return calloc(n, size); }
static inline void *heap_caps_realloc(void *p, size_t n, int caps) { (void)caps; return realloc(p, n); }
static inline void *heap_caps_aligned_alloc(size_t align,size_t n,int caps){(void)caps;void *p=NULL;return posix_memalign(&p,align,n)?NULL:p;}
static inline void heap_caps_free(void *p) { free(p); }
static inline size_t heap_caps_get_largest_free_block(int caps) { (void)caps; return 8u * 1024u * 1024u; }
