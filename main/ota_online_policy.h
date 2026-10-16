/* SPDX-License-Identifier: Apache-2.0
 * 中文：更新清单与版本校验，不访问网络/Flash。
 * English: Manifest/version validation without network or flash access.
 */
#pragma once
#include "ota_online.h"
#include <stddef.h>
typedef struct {
    bool available;
    char version[33], asset_url[160], blob_url[160];
    uint32_t size;
    uint8_t sha256[32];
} if_update_candidate_t;
/// 只接收本产品、固定分区布局及仓库的稳定版清单。/ Accept only stable manifests for this product, layout and repository.
bool if_update_parse_manifest(const char *json, size_t length, if_update_candidate_t *out);
/// 稳定版必须严格更新；兼容旧IF编号及本地test版本。/ Require a strictly newer stable release; accept legacy IF/test current versions.
bool if_update_newer(const char *candidate, const char *current);
/// 仅允许GitHub已知HTTPS资源重定向，不携带账号。/ Permit known GitHub HTTPS asset redirects without credentials.
bool if_update_redirect_allowed(const char *url);
