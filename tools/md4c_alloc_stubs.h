/* SPDX-License-Identifier: Apache-2.0
 * 中文：仅测试md4c内部分配失败。/ English: Test-only md4c allocation failures.
 */
#pragma once
#include <stdlib.h>
void *md_test_malloc(size_t n);
void *md_test_realloc(void *p, size_t n);
#define malloc md_test_malloc
#define realloc md_test_realloc
