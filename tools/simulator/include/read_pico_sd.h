/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
typedef struct {bool present,mounted,space_valid;uint64_t capacity_bytes,total_bytes,free_bytes,filesystem_bytes;int space_error;const char *name;} read_pico_sd_info_t;
esp_err_t read_pico_sd_get_info(read_pico_sd_info_t*);
esp_err_t read_pico_sd_mount(void);
static inline void read_pico_sd_start_probe(void){}
