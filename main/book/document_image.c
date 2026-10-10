/* SPDX-License-Identifier: Apache-2.0
 * 中文：自定义固件 PNG 灰阶解码器。只接受插件输出的非交错 4-bit 灰阶 PNG。
 * English: Derivative grayscale decoder for the plugin's non-interlaced 4-bit gray PNG.
 * 中文：逐个校验 PNG 块 CRC，逐行解压；高度不设屏幕上限，RAM 只保留三行和 zlib 状态。
 * English: Verify chunk CRCs and inflate rows;
 height has no screen cap and RAM holds three rows plus zlib state.
 */
#include "document_image.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>
static uint32_t be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static bool read_exact(FILE *f, void *out, size_t n) {
    return fread(out, 1, n, f) == n;
}
static unsigned paeth(unsigned a, unsigned b, unsigned c) {
    int p = (int)a + (int)b - (int)c;
    int pa = abs(p - (int)a), pb = abs(p - (int)b), pc = abs(p - (int)c);
    return pa <= pb && pa <= pc ? a : pb <= pc ? b : c;
}
bool document_image_open(const char *path, document_image_t *image) {
    if (!path || !image)
        return false;
    memset(image, 0, sizeof(*image));
    FILE *f = fopen(path, "rb");
    if (!f)
        return false;
    uint8_t signature[8];
    bool ok = read_exact(f, signature, 8) && !memcmp(signature, "\x89PNG\r\n\x1a\n", 8);
    uint8_t input[4096], row[344], previous[342] = {0};
    size_t stride = 0, fill = 0;
    uint32_t rows = 0;
    bool header = false, started = false, ended = false, done = false, idat_closed = false, saw_idat = false;
    z_stream z = {0};
    FILE *cache = NULL;
    char temporary[512] = {0};
    (void)mkdir("/sdcard/markdown/.cache", 0700);
    // 临时文件由 source 路径命名；只在应用串行的打开/加载回调调用。
    // Source-derived temporary names are used only by serialized app open/load callbacks.
    uint32_t key = crc32(0, (const Bytef *)path, (uInt)strlen(path));
    snprintf(temporary, sizeof(temporary), "/sdcard/markdown/.cache/%08lx.part", (unsigned long)key);
    while (ok && !done) {
        uint8_t h[8];
        if (!read_exact(f, h, 8)) {
            ok = false;
            break;
        }
        uint32_t count = be32(h);
        if (count > INT32_MAX) {
            ok = false;
            break;
        }
        bool ihdr = !memcmp(h + 4, "IHDR", 4), idat = !memcmp(h + 4, "IDAT", 4),
             iend = !memcmp(h + 4, "IEND", 4);
        if (!header && !ihdr) {
            ok = false;
            break;
        }
        if (ihdr && (header || count != 13)) {
            ok = false;
            break;
        }
        if (iend && count) {
            ok = false;
            break;
        }
        if (idat && idat_closed) {
            ok = false;
            break;
        }
        if (!idat && saw_idat)
            idat_closed = true;
        if (idat)
            saw_idat = true;
        if (!ihdr && !idat && !iend && !(h[4] & 32)) {
            ok = false;
            break;
        }
        uint32_t crc = crc32(0, h + 4, 4);
        uint32_t remaining = count;
        while (ok && remaining) {
            size_t n = remaining > sizeof(input) ? sizeof(input) : remaining;
            if (!read_exact(f, input, n)) {
                ok = false;
                break;
            }
            remaining -= (uint32_t)n;
            crc = crc32(crc, input, (uInt)n);
            if (ihdr) {
                image->width = be32(input);
                image->height = be32(input + 4);
                if (!image->width || image->width > 684 || !image->height || image->height > INT32_MAX ||
                    input[8] != 4 || input[9] != 0 || input[10] || input[11] || input[12]) {
                    ok = false;
                    break;
                }
                stride = (image->width + 1) / 2;
                header = true;
                cache = fopen(temporary, "wb");
                if (!cache || inflateInit(&z) != Z_OK) {
                    ok = false;
                    break;
                }
                started = true;
            } else if (idat) {
                if (ended) {
                    ok = false;
                    break;
                }
                z.next_in = input;
                z.avail_in = (uInt)n;
                while (ok && z.avail_in) {
                    z.next_out = row + fill;
                    z.avail_out = (uInt)(stride + 1 - fill);
                    uInt before = z.avail_in;
                    uLong old_output = z.total_out;
                    int status = inflate(&z, Z_NO_FLUSH);
                    fill = stride + 1 - z.avail_out;
                    if (status != Z_OK && status != Z_STREAM_END) {
                        ok = false;
                        break;
                    }
                    if (fill == stride + 1) {
                        if (rows >= image->height || row[0] > 4) {
                            ok = false;
                            break;
                        }
                        for (size_t x = 0; x < stride; x++) {
                            unsigned a = x ? row[x] : 0, b = previous[x], c = x ? previous[x - 1] : 0;
                            unsigned predictor = row[0] == 1   ? a
                                                 : row[0] == 2 ? b
                                                 : row[0] == 3 ? (a + b) / 2
                                                 : row[0] == 4 ? paeth(a, b, c)
                                                               : 0;
                            row[x + 1] = (uint8_t)(row[x + 1] + predictor);
                        }
                        if (fwrite(row + 1, 1, stride, cache) != stride) {
                            ok = false;
                            break;
                        }
                        memcpy(previous, row + 1, stride);
                        rows++;
                        fill = 0;
                    }
                    if (status == Z_STREAM_END) {
                        ended = true;
                        if (z.avail_in)
                            ok = false;
                        break;
                    }
                    if (before == z.avail_in && old_output == z.total_out) {
                        ok = false;
                        break;
                    }
                }
            }
        }
        uint8_t sum[4];
        if (!ok || !read_exact(f, sum, 4) || crc != be32(sum)) {
            ok = false;
            break;
        }
        if (iend) {
            done = true;
            ok = header && ended && rows == image->height && !fill;
        }
    }
    if (started)
        inflateEnd(&z);
    if (cache && fclose(cache))
        ok = false;
    if (ok && fgetc(f) != EOF)
        ok = false;
    fclose(f);
    if (ok) {
        snprintf(image->cache, sizeof(image->cache), "/sdcard/markdown/.cache/%08lx.gray",
                 (unsigned long)key);
        ok = rename(temporary, image->cache) == 0;
    }
    if (!ok) {
        unlink(temporary);
        memset(image, 0, sizeof(*image));
    }
    return ok;
}
bool document_image_sample(const document_image_t *image, uint32_t top, uint32_t span, unsigned width,
                           unsigned height, uint8_t *gray) {
    if (!image || !image->width || !image->height || !gray || !width || !height || width > 684 ||
        height > 1216 || top >= image->height || !span)
        return false;
    if (span > image->height - top)
        span = image->height - top;
    FILE *f = fopen(image->cache, "rb");
    if (!f)
        return false;
    uint8_t row[342];
    size_t stride = (image->width + 1) / 2;
    uint32_t last = UINT32_MAX;
    bool ok = true;
    for (unsigned y = 0; y < height; y++) {
        uint32_t source = top + (uint32_t)((uint64_t)y * span / height);
        if (source != last) {
            uint64_t offset = (uint64_t)source * stride;
            if (offset > LONG_MAX || fseek(f, (long)offset, SEEK_SET) || !read_exact(f, row, stride)) {
                ok = false;
                break;
            }
            last = source;
        }
        for (unsigned x = 0; x < width; x++) {
            uint32_t sx = (uint64_t)x * image->width / width;
            gray[(size_t)y * width + x] = (uint8_t)(((sx & 1) ? row[sx / 2] & 15 : row[sx / 2] >> 4) * 17);
        }
    }
    fclose(f);
    return ok;
}

void document_image_remove_cache(const char *path) {
    if (!path)
        return;
    uint32_t key = crc32(0, (const Bytef *)path, (uInt)strlen(path));
    char cache[512];
    snprintf(cache, sizeof(cache), "/sdcard/markdown/.cache/%08lx.gray", (unsigned long)key);
    (void)unlink(cache);
    snprintf(cache, sizeof(cache), "/sdcard/markdown/.cache/%08lx.part", (unsigned long)key);
    (void)unlink(cache);
}
