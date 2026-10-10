/* SPDX-License-Identifier: Apache-2.0
 * 中文：rc83升级能力的显式演示替身，不安装固件。
 * English: Explicit rc83 update demo; no firmware installation.
 */
#include <stdio.h>
#include <string.h>
#include "ota_update.h"
#include "esp_app_desc.h"
#include "settings.h"
#include "read_pico_transfer.h"
const esp_app_desc_t *esp_app_get_description(void){static const esp_app_desc_t desc={.version="0.3.3-rc83-if.1",.project_name="Read_Pico_IF"};return &desc;}
esp_err_t pico_ota_inspect(const char *path,pico_ota_info_t *info){memset(info,0,sizeof(*info));snprintf(info->current_version,sizeof(info->current_version),"%s",esp_app_get_description()->version);snprintf(info->message,sizeof(info->message),"模拟器不安装固件，请在 Pico 上检查升级包");return ESP_ERR_NOT_SUPPORTED;}
esp_err_t pico_ota_install(const char *path,char *message,size_t size){snprintf(message,size,"请在 Pico 上安装 IF Edition 升级包");return ESP_ERR_NOT_SUPPORTED;}
void pico_ota_confirm_running(void){}
esp_err_t read_pico_transfer_export_wifi_backup(read_pico_transfer_wifi_backup_t *out){memset(out,0,sizeof(*out));return ESP_OK;}
bool read_pico_transfer_wifi_backup_valid(const read_pico_transfer_wifi_backup_t *in){return in&&in->configured<=1&&memchr(in->ssid,0,sizeof(in->ssid))&&memchr(in->password,0,sizeof(in->password));}
esp_err_t read_pico_transfer_import_wifi_backup(const read_pico_transfer_wifi_backup_t *in){return read_pico_transfer_wifi_backup_valid(in)?ESP_OK:ESP_ERR_INVALID_ARG;}
