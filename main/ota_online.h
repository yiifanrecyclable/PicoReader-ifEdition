/* SPDX-License-Identifier: Apache-2.0
 * 中文：if-ink OS独立稳定更新源；后台写空闲槽，UI任务管理联网与收尾。
 * English: Independent stable updates; a worker writes the inactive slot, the UI owns network lifecycle.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#define IF_OTA_REPOSITORY_ID "1412628734"
#define IF_OTA_FEED_URL "https://api.github.com/repositories/" IF_OTA_REPOSITORY_ID "/contents/ota/stable.json?ref=main"
#define IF_OTA_PROJECT "Read_Pico_IF"
#define IF_OTA_LAYOUT "if-16m-ota-v1"
#define IF_OTA_MAX_BYTES 4194304u
#define IF_OTA_MANIFEST_LIMIT 4096u

typedef enum {
    IF_UPDATE_IDLE, IF_UPDATE_CHECKING, IF_UPDATE_AVAILABLE, IF_UPDATE_CURRENT,
    IF_UPDATE_DOWNLOADING, IF_UPDATE_VERIFYING, IF_UPDATE_READY, IF_UPDATE_FAILED, IF_UPDATE_CANCELLED
} if_update_state_t;
typedef struct {
    if_update_state_t state; ///< 当前阶段 / Current phase
    bool busy; ///< 包含任务及联网收尾 / Includes worker and network teardown
    uint32_t revision, received, total;
    char current[33], version[33], message[128];
} if_update_status_t;
/// 只读快照，不包含凭据。/ Read-only snapshot without credentials.
void if_update_get_status(if_update_status_t *out);
/// UI任务调用，检查本项目稳定版。/ UI owner starts a stable-release check.
esp_err_t if_update_check(void);
/// 用户明确确认后开始下载及安装。/ Download/install only after explicit confirmation.
esp_err_t if_update_install(void);
/// UI任务有界轮询，收回已完成任务及临时网络。/ UI owner reaps completed work and its temporary network.
void if_update_poll(void);
/// 离开/锁屏先取消并等待后台结束；不更改运行槽。/ Cancel/join before exit or sleep; never modify the running slot.
void if_update_close(void);
