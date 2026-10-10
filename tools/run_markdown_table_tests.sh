#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# 中文：表格解析和分页的隔离宿主回归。/ English: Isolated host table parsing and layout regression.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/markdown-tests
cc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -g -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Itools/book_layout_stubs -Itools/html_text_stubs -Imain/book -Icomponents/md4c \
  tools/markdown_table_host_test.c main/book/book_layout.c main/book/markdown_table.c \
  main/book/markdown_text.c main/book/html_text.c components/md4c/md4c.c -o build/markdown-tests/table
build/markdown-tests/table
