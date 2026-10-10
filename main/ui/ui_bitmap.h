/* SPDX-License-Identifier: Apache-2.0
 * 中文：缓存采用逻辑坐标4bpp；屏幕缓冲采用epdiy的物理坐标，不能直接按逻辑行memcpy。
 * English: Cache logical 4bpp rows independently of epdiy's rotated physical framebuffer.
 */
#pragma once
#include "epdiy.h"
#include <stddef.h>

static inline void ui_bitmap_capture(const uint8_t *fb, EpdRect area, uint8_t *image) {
    int w = epd_width(), h = epd_height();
    size_t stride = (size_t)(area.width + 1) / 2;
    for (int y = 0; y < area.height; ++y) for (int x = 0; x < area.width; ++x) {
        int px = area.x + x, py = area.y + y, swap;
        switch (epd_get_rotation()) {
            case EPD_ROT_PORTRAIT: swap = px; px = w - py - 1; py = swap; break;
            case EPD_ROT_INVERTED_LANDSCAPE: px = w - px - 1; py = h - py - 1; break;
            case EPD_ROT_INVERTED_PORTRAIT: swap = px; px = py; py = h - swap - 1; break;
            default: break;
        }
        uint8_t shade = epd_get_pixel(px, py, w, h, fb) >> 4;
        size_t at = (size_t)y * stride + (size_t)x / 2;
        image[at] = x & 1 ? (image[at] & 0x0f) | (shade << 4)
                          : (image[at] & 0xf0) | shade;
    }
}
static inline void ui_bitmap_draw(uint8_t *fb, EpdRect area, const uint8_t *image) {
    epd_draw_rotated_image(area, image, fb);
}
