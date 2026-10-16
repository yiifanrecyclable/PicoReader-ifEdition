#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实USB收尾回调故障注入，失败保留句柄且不重挂卡。/ Fault-inject production USB teardown, retaining handles and preventing unsafe remounts."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
src=(root/'main/apps/usb_storage.c').read_text()
start=src.index('esp_err_t usb_storage_stop(void) {')
fn=src[start:]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
typedef int esp_err_t;
enum {ESP_OK=0,ESP_FAIL=-1,ESP_ERR_INVALID_STATE=3};
#define pdMS_TO_TICKS(n) (n)
static void *s_storage;
static bool s_active,s_cleanup_pending,s_usb_ready,s_msc_ready,s_host_ready;
static int fault,remounts,restores,delete_calls;
static void restore_serial_phy(void){++restores;}
static void restore_local(void){assert(!s_storage&&!s_usb_ready&&!s_msc_ready);++remounts;s_host_ready=false;}
static void tud_disconnect(void){}
static void vTaskDelay(int ticks){(void)ticks;}
static int tinyusb_msc_delete_storage(void *p){assert(p);++delete_calls;return fault==1?ESP_ERR_INVALID_STATE:ESP_OK;}
static int tinyusb_driver_uninstall(void){return fault==2?ESP_FAIL:ESP_OK;}
static int tinyusb_msc_uninstall_driver(void){return fault==3?ESP_FAIL:ESP_OK;}
'''+fn+r'''
int main(void){
 for(int f=1;f<=3;++f){
  s_storage=(void*)1;s_active=s_usb_ready=s_msc_ready=s_host_ready=true;s_cleanup_pending=false;
  int before=remounts;fault=f;
  assert(usb_storage_stop()!=ESP_OK&&s_active&&s_cleanup_pending&&remounts==before);
  if(f==1)assert(s_storage&&delete_calls>=20);
  fault=0;assert(usb_storage_stop()==ESP_OK&&!s_active&&!s_cleanup_pending&&remounts==before+1);
 }
 // 启动失败留下的半初始化状态也必须释放，而非因inactive提前返回。
 // Partially initialized failed starts must clean up even when inactive.
 s_storage=(void*)1;s_active=false;s_msc_ready=s_host_ready=true;
 assert(usb_storage_stop()==ESP_OK&&!s_storage&&!s_active&&!s_cleanup_pending);
 puts("USB cleanup: delete/uninstall faults preserve exclusivity; retries and failed-start teardown passed");
}
'''
with tempfile.TemporaryDirectory(prefix='usb-cleanup-',dir=root/'build') as folder:
    p=Path(folder);(p/'test.c').write_text(unit)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p/'test.c'),'-o',str(p/'test')],check=True)
    subprocess.run([str(p/'test')],check=True)
