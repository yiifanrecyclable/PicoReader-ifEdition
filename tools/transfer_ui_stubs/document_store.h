/* SPDX-License-Identifier: Apache-2.0
 * 文档入库由独立存储测试验证，此处只检查页面接线。
 * Document storage has separate tests; this stub checks page wiring only.
 */
#pragma once
#include "esp_err.h"
esp_err_t document_store_import_local(const char *staging, const char *destination);
