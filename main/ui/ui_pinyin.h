/* SPDX-License-Identifier: Apache-2.0
 * 基础单字及可选TF词组共用候选服务。/ Shared candidates: built-in characters plus optional TF phrases.
 */
#pragma once
#include <stddef.h>
#define UI_PINYIN_WORD_BYTES 13
#ifndef UI_PINYIN_DICTIONARY_PATH
#define UI_PINYIN_DICTIONARY_PATH "/sdcard/system/ime/pinyin.ifdict"
#endif
size_t ui_pinyin_candidates(const char *pinyin, char (*out)[UI_PINYIN_WORD_BYTES], size_t capacity, size_t skip);
