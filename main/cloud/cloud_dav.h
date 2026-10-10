/* SPDX-License-Identifier: Apache-2.0
 * 中文：流式WebDAV目录解析；只接受收件目录直属可读文件。
 * English: Streaming WebDAV listing parser for direct readable inbox children.
 */
#pragma once
#include "cloud_protocol.h"
#include <stdio.h>
typedef struct {
    char href[241], etag[129], created_at[25], modified[65];
    uint64_t bytes, created_ms;
    bool manifest, date_fallback; ///< manifest退休字段恒为false；保留宿主快照兼容 / Retired manifest flag is always false, retained for host snapshots
} cloud_dav_entry_t;
esp_err_t cloud_dav_parse(FILE *xml, const char *decoded_folder_path, cloud_dav_entry_t *out, size_t capacity,
                          size_t *count, bool *truncated);
