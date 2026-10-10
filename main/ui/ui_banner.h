/* SPDX-License-Identifier: Apache-2.0
 * 中文：首页大字格言的两行限制与固定锚点。 / English: Two-line large home text with fixed anchors.
 */
#pragma once
#include "content_library.h"
#include <stdbool.h>
#define UI_BANNER_QUOTE_PX 96
#define UI_BANNER_GAP 14
#define UI_BANNER_BOTTOM 370
typedef struct {char lines[2][CONTENT_HOME_QUOTE_BYTES];int count;bool overflow;} ui_banner_quote_t;
typedef struct {int date_px,date_top,brand_top,quote_top,quote_height,quote_px;content_quote_align_t quote_align;} ui_banner_positions_t;
/// 在当前系统字库中计算锚点，格言换行不改变前三行位置。/ Compute system-face anchors independent of quote wrapping.
ui_banner_positions_t ui_banner_positions(const char *date);
ui_banner_positions_t ui_banner_positions_size(const char *date,int px);
bool ui_banner_quote_fits_size(const char *text,const char *path,int px);
/// 返回逻辑竖屏坐标的300行4bpp缓存；用ui_bitmap_draw绘制，不能直接复制进物理屏幕缓冲。
/// Return 300 logical portrait rows; draw with ui_bitmap_draw, never memcpy into the physical framebuffer.
bool ui_banner_preview(const char *text,const char *path,int px,content_quote_align_t align,uint32_t day,bool valid,uint8_t **out,bool *fits);
/// 品牌行与两侧细横线共用绘制，整体两端略窄于模块分割线。/ Draw the centered brand with side rules inset from module dividers.
void ui_banner_brand_draw(uint8_t *fb,ui_banner_positions_t positions);
void ui_banner_date_line(uint32_t day,bool valid,char *line,size_t cap);
bool ui_banner_quote_layout(const char *text,bool custom,ui_banner_quote_t *out);
bool ui_banner_quote_fits_font(const char *text,const char *path);
void ui_banner_quote_draw(uint8_t *fb,const char *text,bool custom,ui_banner_positions_t positions);
