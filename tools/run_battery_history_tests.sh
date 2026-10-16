#!/usr/bin/env bash
# SPDX-License-Identifier: Apache-2.0
# 中文：所有电量记录写入临时目录；不访问用户TF卡。/ All history writes use a temporary directory, never the user's TF card.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build
battery_test_dir=$(mktemp -d "$PWD/build/battery-test.XXXXXX")
trap 'rm -rf "$battery_test_dir"' EXIT
cc -std=c11 -D_DEFAULT_SOURCE -DBATTERY_HISTORY_DIR='"history"' -Wall -Wextra -Werror -g \
 -fsanitize=address,undefined -fno-omit-frame-pointer -Itools/html_text_stubs -Imain/power \
 tools/battery_history_host_test.c main/power/battery_history.c -o "$battery_test_dir/test"
(cd "$battery_test_dir" && ./test)
