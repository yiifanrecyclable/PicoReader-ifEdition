/* SPDX-License-Identifier: Apache-2.0
 * 中文：仅接受插件v1/v2的完整有界清单，拒绝重复字段、穿越、超量与不匹配资源。
 * English: Accept complete bounded v1/v2 manifests;
 reject duplicate keys, traversal, oversized and mismatched resources.
 */
#include "cloud_protocol.h"
#include "cJSON.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
bool cloud_utf8_valid(const char *text, size_t length) {
    const unsigned char *p = (const unsigned char *)text;
    size_t i = 0;
    while (i < length) {
        unsigned c = p[i++];
        if (!c)
            return false;
        if (c < 128)
            continue;
        unsigned count, minimum;
        if (c >= 0xc2 && c <= 0xdf) {
            count = 1;
            minimum = 128;
            c &= 31;
        } else if (c >= 0xe0 && c <= 0xef) {
            count = 2;
            minimum = 2048;
            c &= 15;
        } else if (c >= 0xf0 && c <= 0xf4) {
            count = 3;
            minimum = 65536;
            c &= 7;
        } else
            return false;
        if (i + count > length)
            return false;
        while (count--) {
            unsigned next = p[i++];
            if ((next & 0xc0) != 0x80)
                return false;
            c = c * 64 + (next & 63);
        }
        if (c < minimum || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff))
            return false;
    }
    return true;
}
static bool hex_id(const char *value) {
    if (!value || strlen(value) != 64)
        return false;
    for (unsigned i = 0; i < 64; ++i)
        if (!(value[i] >= '0' && value[i] <= '9') && !(value[i] >= 'a' && value[i] <= 'f'))
            return false;
    return true;
}
bool cloud_relative_path(const char *path) {
    if (!path || !*path || strlen(path) > 240 || *path == '/' || strchr(path, '\\') ||
        !cloud_utf8_valid(path, strlen(path)))
        return false;
    const char *p = path;
    while (*p) {
        const char *end = strchr(p, '/');
        if (!end)
            end = p + strlen(p);
        size_t n = end - p;
        if (!n || (n == 1 && *p == '.') || (n == 2 && !memcmp(p, "..", 2)))
            return false;
        for (const unsigned char *k = (const unsigned char *)p; k < (const unsigned char *)end; ++k)
            if (*k < 32 || *k == 127)
                return false;
        if (!*end)
            break;
        p = end + 1;
        if (!*p)
            return false;
    }
    return true;
}
bool cloud_encode_path(const char *path, char *encoded, size_t cap) {
    if (!encoded || !cap || !cloud_relative_path(path))
        return false;
    size_t n = 0;
    static const char hex[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)path; *p; ++p) {
        bool plain = (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9') ||
                     strchr("/-._~", *p);
        size_t need = plain ? 1 : 3;
        if (n + need >= cap)
            return false;
        if (plain)
            encoded[n++] = *p;
        else {
            encoded[n++] = '%';
            encoded[n++] = hex[*p >> 4];
            encoded[n++] = hex[*p & 15];
        }
    }
    encoded[n] = 0;
    return true;
}
static int digits(const char *p, int n) {
    int value = 0;
    while (n--) {
        if (*p < '0' || *p > '9')
            return -1;
        value = value * 10 + *p++ - '0';
    }
    return value;
}
bool cloud_timestamp(const char *value, uint64_t *ms) {
    if (!value)
        return false;
    size_t n = strlen(value);
    if ((n != 20 && n != 24) || value[4] != '-' || value[7] != '-' || value[10] != 'T' || value[13] != ':' ||
        value[16] != ':' || value[n - 1] != 'Z' || (n == 24 && value[19] != '.'))
        return false;
    int y = digits(value, 4), m = digits(value + 5, 2), d = digits(value + 8, 2), h = digits(value + 11, 2),
        minute = digits(value + 14, 2), sec = digits(value + 17, 2),
        fraction = n == 24 ? digits(value + 20, 3) : 0;
    static const int lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    bool leap = y % 4 == 0 && (y % 100 != 0 || y % 400 == 0);
    if (y < 2000 || y > 2099 || m < 1 || m > 12 || d < 1 || d > lengths[m - 1] + (m == 2 && leap) || h < 0 ||
        h > 23 || minute < 0 || minute > 59 || sec < 0 || sec > 59 || fraction < 0)
        return false;
    uint64_t days = 10957;
    for (int year = 2000; year < y; ++year)
        days += 365 + (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
    for (int month = 1; month < m; ++month)
        days += lengths[month - 1] + (month == 2 && leap);
    days += d - 1;
    if (ms)
        *ms = ((days * 24 + h) * 60 + minute) * 60000 + sec * 1000 + fraction;
    return true;
}
static bool json_preflight(const char *text, size_t n) {
    if (!cloud_utf8_valid(text, n))
        return false;
    bool quoted = false, escaped = false;
    int depth = 0;
    for (size_t i = 0; i < n; ++i) {
        char c = text[i];
        if (quoted) {
            if (escaped) {
                if (c == 'u' && i + 4 < n && !memcmp(text + i + 1, "0000", 4))
                    return false;
                escaped = false;
            } else if (c == '\\')
                escaped = true;
            else if (c == '"')
                quoted = false;
        } else if (c == '"')
            quoted = true;
        else if (c == '{' || c == '[') {
            if (++depth > 16)
                return false;
        } else if (c == '}' || c == ']') {
            if (--depth < 0)
                return false;
        }
    }
    return !quoted && !escaped && depth == 0;
}
static bool unique_keys(const cJSON *node, unsigned depth) {
    if (depth > 16)
        return false;
    if (cJSON_IsObject(node))
        for (const cJSON *a = node->child; a; a = a->next) {
            if (!a->string)
                return false;
            for (const cJSON *b = a->next; b; b = b->next)
                if (!b->string || !strcmp(a->string, b->string))
                    return false;
        }
    for (const cJSON *child = node->child; child; child = child->next)
        if (!unique_keys(child, depth + 1))
            return false;
    return true;
}
static const char *string_key(const cJSON *node, const char *key) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(node, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}
static bool text_key(const cJSON *node, const char *key, char *out, size_t cap) {
    const char *v = string_key(node, key);
    if (!v || strlen(v) >= cap || !cloud_utf8_valid(v, strlen(v)))
        return false;
    strcpy(out, v);
    return true;
}
static bool uint_key(const cJSON *node, const char *key, uint32_t maximum, uint32_t *out) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(node, key);
    if (!cJSON_IsNumber(v) || !isfinite(v->valuedouble) || v->valuedouble < 0 || v->valuedouble > maximum ||
        floor(v->valuedouble) != v->valuedouble)
        return false;
    *out = (uint32_t)v->valuedouble;
    return true;
}
esp_err_t cloud_manifest_parse(const char *json, size_t length, const char *expected_id,
                               cloud_manifest_t *out) {
    if (!json || !out || !hex_id(expected_id) || length > CLOUD_MANIFEST_MAX || !json_preflight(json, length))
        return ESP_ERR_INVALID_RESPONSE;
    memset(out, 0, sizeof(*out));
    char *buffer = malloc(length + 1);
    if (!buffer)
        return ESP_ERR_NO_MEM;
    memcpy(buffer, json, length);
    buffer[length] = 0;
    cJSON *root = cJSON_ParseWithLengthOpts(buffer, length + 1, NULL, 1);
    free(buffer);
    if (!root)
        return ESP_ERR_INVALID_RESPONSE;
    bool ok = cJSON_IsObject(root) && unique_keys(root, 0);
    uint32_t version = 0;
    ok = ok && uint_key(root, "schema_version", 2, &version) && version >= 1 &&
         text_key(root, "article_id", out->article_id, sizeof(out->article_id)) &&
         !strcmp(out->article_id, expected_id) &&
         text_key(root, "revision_id", out->revision_id, sizeof(out->revision_id)) &&
         hex_id(out->revision_id) && text_key(root, "title", out->title, sizeof(out->title)) &&
         out->title[0] && text_key(root, "created_at", out->created_at, sizeof(out->created_at)) &&
         text_key(root, "updated_at", out->updated_at, sizeof(out->updated_at)) &&
         cloud_timestamp(out->created_at, &out->created_ms) &&
         cloud_timestamp(out->updated_at, &out->updated_ms) && out->created_ms <= out->updated_ms;
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "body"),
                *assets = cJSON_GetObjectItemCaseSensitive(root, "assets");
    const char *mime = string_key(body, "mime");
    ok = ok && cJSON_IsObject(body) && mime && !strcmp(mime, "text/markdown") &&
         text_key(body, "path", out->body_path, sizeof(out->body_path)) &&
         text_key(body, "sha256", out->body_sha256, sizeof(out->body_sha256)) && hex_id(out->body_sha256) &&
         uint_key(body, "bytes", 256 * 1024, &out->body_bytes) && cJSON_IsArray(assets);
    char expected[241];
    snprintf(expected, sizeof(expected), "_pico/%s/%s/body.md", out->article_id, out->revision_id);
    ok = ok && !strcmp(out->body_path, expected) && cloud_relative_path(out->body_path);
    int count = cJSON_IsArray(assets) ? cJSON_GetArraySize(assets) : -1;
    ok = ok && count >= 0 && count <= 64 &&
         ((version == 1 && count == 0 && length <= 4096 && !strcmp(out->body_sha256, out->revision_id)) ||
          (version == 2 && count > 0));
    uint64_t total = 0;
    snprintf(expected, sizeof(expected), "_pico/%s/%s/attachments/", out->article_id, out->revision_id);
    size_t prefix = strlen(expected);
    for (int i = 0; ok && i < count; ++i) {
        const cJSON *a = cJSON_GetArrayItem(assets, i);
        cloud_asset_t *target = &out->assets[i];
        mime = string_key(a, "mime");
        ok = cJSON_IsObject(a) && mime && !strcmp(mime, "image/png") &&
             text_key(a, "path", target->path, sizeof(target->path)) && cloud_relative_path(target->path) &&
             !strncmp(target->path, expected, prefix) &&
             document_store_safe_filename(target->path + prefix) &&
             text_key(a, "sha256", target->sha256, sizeof(target->sha256)) && hex_id(target->sha256) &&
             uint_key(a, "bytes", 17 * 1024 * 1024, &target->bytes) && target->bytes >= 67 &&
             uint_key(a, "width", 684, &target->width) && target->width &&
             uint_key(a, "height", 32 * 1024 * 1024, &target->height) && target->height &&
             (uint64_t)target->width * target->height <= 32 * 1024 * 1024;
        size_t n = strlen(target->path + prefix);
        if (n < 22 || strcmp(target->path + prefix + n - 4, ".png"))
            ok = false;
        // Attachment suffix: article filename plus sixteen lowercase random hex digits.
        const char *base = target->path + prefix;
        if (n >= 22) {
            if (base[n - 21] != '-')
                ok = false;
            for (size_t k = n - 20; k < n - 4; ++k)
                if (!(base[k] >= '0' && base[k] <= '9') && !(base[k] >= 'a' && base[k] <= 'f'))
                    ok = false;
        }
        const cJSON *source = cJSON_GetObjectItemCaseSensitive(a, "source_sha256");
        if (source)
            ok = ok && text_key(a, "source_sha256", target->source_sha256, sizeof(target->source_sha256)) &&
                 hex_id(target->source_sha256);
        for (int k = 0; k < i; ++k)
            if (!strcmp(out->assets[k].path, target->path))
                ok = false;
        total += target->bytes;
        if (total > 32 * 1024 * 1024)
            ok = false;
    }
    if (ok && version == 2) {
        cJSON *revision = cJSON_CreateObject(), *array = cJSON_CreateArray();
        ok = revision && array && cJSON_AddStringToObject(revision, "body", out->body_sha256) &&
             cJSON_AddItemToObject(revision, "assets", array);
        if (!ok) {
            cJSON_Delete(revision);
            cJSON_Delete(array);
        } else {
            for (int i = 0; ok && i < count; ++i) {
                cJSON *a = cJSON_CreateObject();
                const cloud_asset_t *asset = &out->assets[i];
                if (!a) {
                    ok = false;
                    break;
                }
                ok = cJSON_AddItemToArray(array, a) &&
                     cJSON_AddStringToObject(a, "name", strrchr(asset->path, '/') + 1) &&
                     cJSON_AddStringToObject(a, "sha256", asset->sha256);
                if (ok && asset->source_sha256[0])
                    ok = cJSON_AddStringToObject(a, "source_sha256", asset->source_sha256);
            }
            char *encoded = ok ? cJSON_PrintUnformatted(revision) : NULL;
            char hash[65];
            if (encoded) {
                bool hashed = document_store_hash_bytes(encoded, strlen(encoded), hash);
                ok = hashed && !strcmp(hash, out->revision_id);
                free(encoded);
            } else
                ok = false;
            cJSON_Delete(revision);
        }
    }
    bool filename_present = cJSON_GetObjectItemCaseSensitive(root, "source_filename") != NULL;
    if (filename_present)
        ok = ok && text_key(root, "source_filename", out->filename, sizeof(out->filename)) &&
             document_store_safe_filename(out->filename) && strrchr(out->filename, '.') &&
             !strcasecmp(strrchr(out->filename, '.'), ".md");
    out->version = version;
    out->asset_count = count > 0 ? (size_t)count : 0;
    cJSON_Delete(root);
    if (!ok)
        return ESP_ERR_INVALID_RESPONSE;
    return filename_present ? ESP_OK : ESP_ERR_NOT_SUPPORTED;
}

// 坚果云DAV属性和GET可能均不带引号，不把裸串当作标准If-Match条件。
// Nutstore may omit quotes in both DAV and GET; bare tags are not used as standard If-Match conditions.
bool cloud_etag_version(const char *value, char opaque[129]) {
    if (!value)
        return false;
    size_t n = strnlen(value, 129);
    if (!n || n > 128 || !strncmp(value, "W/", 2))
        return false;
    const char *begin = value;
    size_t length = n;
    if (n >= 2 && value[0] == '"' && value[n - 1] == '"') {
        ++begin;
        length -= 2;
    }
    if (!length)
        return false;
    for (size_t i = 0; i < length; ++i)
        if ((unsigned char)begin[i] < 33 || (unsigned char)begin[i] > 126 || begin[i] == '"')
            return false;
    if (opaque) {
        memcpy(opaque, begin, length);
        opaque[length] = 0;
    }
    return true;
}
bool cloud_etag_same(const char *expected, const char *actual) {
    char a[129], b[129];
    return cloud_etag_version(expected, a) && cloud_etag_version(actual, b) && !strcmp(a, b);
}
