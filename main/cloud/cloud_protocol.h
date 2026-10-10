/* SPDX-License-Identifier: Apache-2.0
 * 中文：坚果云文章清单及相对路径的有界解析，不访问凭据或网络。
 * English: Bounded cloud article and relative-path validation, independent of credentials and networking.
 */
#pragma once
#include "document_store.h"
#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define CLOUD_ENTRY_MAX 96
#define CLOUD_MANIFEST_MAX 32768
#define CLOUD_XML_MAX (512 * 1024)
typedef struct {
    char path[241], sha256[65], source_sha256[65];
    uint32_t bytes, width, height;
} cloud_asset_t;
typedef struct {
    unsigned version;
    char article_id[65], revision_id[65], title[241], filename[241];
    char created_at[25], updated_at[25], body_path[241], body_sha256[65];
    uint32_t body_bytes;
    uint64_t created_ms, updated_ms;
    size_t asset_count;
    cloud_asset_t assets[64];
} cloud_manifest_t;
bool cloud_utf8_valid(const char *text, size_t length);
bool cloud_relative_path(const char *path);
bool cloud_encode_path(const char *path, char *encoded, size_t capacity);
esp_err_t cloud_manifest_parse(const char *json, size_t length, const char *expected_id,
                               cloud_manifest_t *out);
bool cloud_timestamp(const char *value, uint64_t *milliseconds);

/// 规范引号/坚果云无引号版本串，拒绝弱标记与控制字符。/ Normalize quoted or Nutstore opaque tags; reject
/// weak/invalid tags.
bool cloud_etag_version(const char *value, char opaque[129]);
/// 比较有效版本串，不把空值当作相同版本。/ Compare valid opaque versions; empty values never match.
bool cloud_etag_same(const char *expected, const char *actual);
