/* SPDX-License-Identifier: Apache-2.0
 * 中文：有界Markdown转阅读块，固件与宿主使用同一实现。
 * English: Bounded Markdown reading blocks shared by firmware and host.
 * 冻结：不执行HTML或脚本，不访问文件或网络；图片暂为可见文字占位。
 * Frozen: No HTML execution, files or networking; images are visible text placeholders.
 */
#pragma once
#include "html_text.h"
#define MARKDOWN_INPUT_MAX (256u * 1024u)
#define MARKDOWN_TEXT_MAX (1024u * 1024u)
#define MARKDOWN_BLOCK_MAX 8192u
#define MARKDOWN_IMAGE_MAX 64u
#define MARKDOWN_DEPTH_MAX 64u
#define MARKDOWN_PATH_MAX 240u
/// out必须为空，成功交出所有权；失败清空；拒绝NUL与非法UTF8。
/// out must be empty; transfer ownership on success, clear on failure, reject NUL/invalid UTF-8.
esp_err_t markdown_to_blocks(const char *input, size_t len, html_text_t *out);
