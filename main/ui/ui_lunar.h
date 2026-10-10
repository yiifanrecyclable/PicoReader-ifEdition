/* SPDX-License-Identifier: Apache-2.0
 * 中文：离线农历日期显示，无联网/黄历功能。 / English: Offline lunar date labels, without network/almanac features.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
typedef struct { int year, month, day; bool leap; } ui_lunar_date_t;
/// UTC+8本地日序转农历；表范围外返回false。/ Convert a UTC+8 local day index; false outside the table range.
bool ui_lunar_date(int32_t day, ui_lunar_date_t *out);
bool ui_lunar_label(int32_t day, char *out, size_t cap);
