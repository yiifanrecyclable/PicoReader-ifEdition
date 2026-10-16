/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 图书进度换算测试。/ Book position mapping tests.
 */
#include <assert.h>
#include <stdio.h>
#include "book_policy.h"

int main(void) {
    assert(book_position_bytes(100, 200, 50, 100) == 150);
    assert(book_position_bytes(100, 200, 150, 100) == 200);
    assert(book_position_bytes(100, 200, 0, 0) == 100);
    assert(book_position_bytes(0, UINT32_MAX, UINT32_MAX, UINT32_MAX) == UINT32_MAX);
    puts("PASS book position mapping");
}
