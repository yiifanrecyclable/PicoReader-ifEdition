/* SPDX-License-Identifier: Apache-2.0
 * English: Host adapter for the production QR API, backed by Nayuki MIT QR generation.
 */
#pragma once
#include "esp_err.h"
#include "../vendor/qrcodegen.h"
typedef const uint8_t *esp_qrcode_handle_t;
typedef struct {void(*display_func)(esp_qrcode_handle_t);} esp_qrcode_config_t;
#define ESP_QRCODE_CONFIG_DEFAULT() ((esp_qrcode_config_t){0})
static inline int esp_qrcode_get_size(esp_qrcode_handle_t q){return qrcodegen_getSize(q);}
static inline bool esp_qrcode_get_module(esp_qrcode_handle_t q,int x,int y){return qrcodegen_getModule(q,x,y);}
static inline esp_err_t esp_qrcode_generate(const esp_qrcode_config_t *cfg,const char *text){
    uint8_t qr[qrcodegen_BUFFER_LEN_FOR_VERSION(10)],temp[qrcodegen_BUFFER_LEN_FOR_VERSION(10)];
    if(!qrcodegen_encodeText(text,temp,qr,qrcodegen_Ecc_MEDIUM,1,10,qrcodegen_Mask_AUTO,true))return ESP_ERR_INVALID_SIZE;
    if(cfg->display_func)cfg->display_func(qr);return ESP_OK;
}
