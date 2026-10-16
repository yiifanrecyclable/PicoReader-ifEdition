/* SPDX-License-Identifier: Apache-2.0
 * 中文：保存真实电量采样；不测量或推算分功能耗电。
 * English: Persist measured battery samples; never infer per-feature energy usage.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define BATTERY_HISTORY_MAX 672
#define BATTERY_HISTORY_INTERVAL 900U
#define BATTERY_HISTORY_WINDOW (7U * 86400U)
typedef struct {
    uint32_t at;
    uint16_t millivolts;
    uint8_t percent;
    uint8_t charging;
} battery_sample_t;
/// 只在已有PMU采样/可用TF上执行；不访问I2C或唤醒睡眠设备。
/// Observe cached PMU readings and available TF only; never access I2C or wake the device.
void battery_history_observe(uint32_t at, int percent, unsigned millivolts, bool charging, bool media_ready);
/// 时间有序、只读的当前卡记录；无卡仍可记录本次会话。
/// Ordered read-only history for this card; an unmounted card allows session-only samples.
const battery_sample_t *battery_history_samples(size_t *count);
/// 测试/换卡时释放会话状态。/ Release session state for tests or card replacement.
void battery_history_reset(void);
