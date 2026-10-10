/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
typedef int esp_log_level_t;
#define ESP_LOG_WARN 2
static inline esp_log_level_t esp_log_level_get(const char *tag){(void)tag;return 3;}
static inline void esp_log_level_set(const char *tag,esp_log_level_t level){(void)tag;(void)level;}
#include <stdio.h>
#define ESP_LOGE(tag, fmt, ...) fprintf(stderr, "[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) fprintf(stderr, "[%s] " fmt "\n", tag, ##__VA_ARGS__)
#define ESP_LOGI(tag, fmt, ...) do { if (0) fprintf(stderr, "[%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
#define ESP_LOGD(tag, fmt, ...) do { if (0) fprintf(stderr, "[%s] " fmt "\n", tag, ##__VA_ARGS__); } while (0)
