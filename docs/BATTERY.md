# 电池管理 / Battery management

设置 → 阅读与设备 → 电池管理：显示当前电量、电压、充电状态，支持近24小时和近七天电量折线图。
Settings → Reading and device → Battery management: current charge, voltage and charging status, with 24-hour and seven-day history.

使用PMU已有的有效电量快照。只在设备运行时每15分钟记录一次，不为采样额外唤醒设备或发起新的I2C读取。需先正确设置时间；未知电量不进入历史。休眠、关机以及超过30分钟的采样空档留白，不插值补齐。
Use the PMU's existing valid cached gauge. Sample every fifteen minutes while running, without waking the device or adding I2C reads. A valid clock is required; reject unknown readings. Leave sleep, shutdown and gaps exceeding thirty minutes blank, without interpolation.

最多保存672个真实样本，位于TF卡隐藏目录 `.if-ink/battery-0.bin` 和 `battery-1.bin`。双槽交替写入并回读核验；新槽不完整或损坏时保留另一个有效槽。内存及临时回读缓冲使用PSRAM，不占用设置NVS。没有卡时仅保留本次运行中的记录，换卡不合并前一张卡的历史。电量历史不属于“保存与恢复”的个人设置备份。
Keep at most 672 measured samples in hidden TF files `.if-ink/battery-0.bin` and `battery-1.bin`. Alternate slots and verify readback; an incomplete/corrupt slot does not replace the other valid copy. Use PSRAM and no settings NVS. Cardless samples last only for the session; changing cards does not merge histories. Battery history is separate from the settings backup.

当前PMU接口提供软件估计电量、电压和充电状态，没有分功能电流/功耗测量。因此不显示各功能耗电排行或伪造精确续航预测。模拟器可展示明确标注的示例曲线，示例不会写入TF卡或作为硬件采样。
The current PMU interface exposes estimated charge, voltage and charging status without per-feature current/power measurements. Do not present feature energy rankings or fabricated precise endurance estimates. The simulator may show explicitly labeled sample curves; never persist them as hardware measurements.

设置 → 关机睡眠 → 无操作后休眠：关闭、1/3/5/10/30分钟，默认5分钟。触摸、按键重新计时；后台刷新不算操作。WiFi/热点传输会话、USB占用、坚果云及微读后台任务暂停计时，结束后重新开始完整间隔。到时先保存页面状态，使用既有壁纸锁屏/休眠路径；唤醒返回原页。配置随备份保存，旧备份保留当前值。
Settings → Shutdown/sleep → Idle sleep: off or 1/3/5/10/30 minutes, default 5. Input resets the timer; redraws do not. Transfer sessions, USB ownership and cloud/WeRead workers suspend timing and restart the full interval afterward. Save page state before the existing wallpaper lock/sleep path and resume the page on wake. Include the preference in backups; legacy backups retain the current value.
