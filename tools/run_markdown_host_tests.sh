#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/markdown-tests
cc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -Icomponents/md4c -include tools/md4c_alloc_stubs.h \
    -c components/md4c/md4c.c -o build/markdown-tests/md4c.o
cc -std=c11 -D_DEFAULT_SOURCE -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -fno-omit-frame-pointer -Itools/html_text_stubs -Imain/book -Icomponents/md4c \
    tools/markdown_host_test.c main/book/markdown_text.c main/book/html_text.c \
    build/markdown-tests/md4c.o -o build/markdown-tests/parser
build/markdown-tests/parser
