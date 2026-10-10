/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：宿主布局测试的内存适配。/ English: Memory adapter for host layout tests.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#pragma once
#include <stdlib.h>
#include <stdbool.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern int layout_test_fail_after;
static inline bool layout_allocation_allowed(void) {
    if (layout_test_fail_after == 0) return false;
    if (layout_test_fail_after > 0) --layout_test_fail_after;
    return true;
}
static inline void* heap_caps_malloc(size_t n, int caps) { (void)caps; return layout_allocation_allowed() ? malloc(n) : NULL; }
static inline void* heap_caps_calloc(size_t n, size_t size, int caps) { (void)caps; return layout_allocation_allowed() ? calloc(n,size) : NULL; }
static inline void* heap_caps_realloc(void* p, size_t n, int caps) { (void)caps; return layout_allocation_allowed() ? realloc(p, n) : NULL; }
