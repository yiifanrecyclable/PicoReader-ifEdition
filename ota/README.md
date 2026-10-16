# if-ink OS 在线升级 / Online updates

稳定通道：`ota/stable.json`。设备只在用户点击“在线升级”时检查，确认后才安装。稳定通道当前发布 if-ink OS v0.1.1；旧 IF 版本和开发测试包不会自动成为稳定通道目标。

Stable feed: `ota/stable.json`. Devices check on request and install after confirmation. It remains disabled until the first tested OS stable release; legacy IF releases and development builds are excluded.

- 固定仓库编号 `1412628734`，设备清单地址为 `https://api.github.com/repositories/1412628734/contents/ota/stable.json?ref=main`，原始文件 Accept 为 `application/vnd.github.raw+json`。仓库改名后编号不变。
- 应用身份保留 `Read_Pico_IF`；独立系统名称为 if-ink OS。布局 `if-16m-ota-v1`：两个4MiB应用槽、原有OTA数据分区。只升级应用，不改分区表、bootloader或用户数据，不依赖TF卡。
- 固件优先通过同一仓库的不可变Git blob编号直接下载，避开Release CDN；连接失败再尝试数字asset ID的Release备用线路。限定HTTPS和GitHub资源重定向；校验芯片、项目、版本、大小及SHA256，ESP-IDF验证完整镜像后才切换启动槽。失败可重试，当前运行槽保持；新版本首次启动按既有逻辑确认/回退。
- 首次使用此功能前须先安装包含更新器的新固件。模拟器只预览操作，不执行网络更新或刷写。

Repository ID survives renames. The retained `Read_Pico_IF` image identity and two-slot layout preserve compatibility. Application-only OTA does not alter the bootloader, partition table or user data. HTTPS, image identity, size, SHA256 and ESP-IDF image validation precede activation. Repository content-addressed downloads are preferred over the Release CDN, with the release asset as a connection-failure fallback. SHA256 detects corruption; the release account and HTTPS feed remain the trust boundary. No separate signing key is introduced. The simulator never installs firmware.

## 发布稳定版 / Publishing stable releases

1. 用户验收并授权发布后，再制作固件。版本使用 `0.1.0`、`0.1.1` 等稳定编号；`-test.N` 不入稳定通道。
2. 在同一仓库创建对应稳定Release（例如 `v0.1.1`），提供对应源码、许可和应用`.bin`。在线升级不能使用合并USB镜像、ZIP或分区表。
3. 获取该Release元数据（`gh api repositories/1412628734/releases/tags/v0.1.1`），保存到本地JSON。
4. 将与Release相同的应用`.bin`提交到本站在线刷机目录（沿用现有webflash发布流程）；必须是同一份字节，工具自动生成不可变blob编号。运行 `python3 tools/prepare_online_update.py APP.bin --release-json RELEASE.json --asset-id ID --out ota/stable.json`。工具校验实际镜像、版本与GitHub资产SHA256，仅生成清单。
5. 先确认同版首次安装/TF升级包及源码下载可用，再将新清单提交到main；这是向已安装设备开放更新的最后一步。撤回更新可将`available`恢复为false，已安装设备不会降级。

After approval, release a stable application binary with matching source/licenses, fetch that release's metadata, and generate the manifest with the command above. The helper checks image checksum/hash, version, project and the published asset digest. Commit the feed only after downloads/source are verified. Disabling availability withdraws the offer without downgrading installed devices.

失败后不做后台无限重试或自动降级；GitHub连不上/被限流时提示重试。临时联网由UI任务回收，升级期间暂停无操作休眠。下载进度最多每秒灰阶差分一次。
Failures do not cause infinite retries or automatic downgrade. The UI reports connectivity/rate-limit failures, owns temporary WiFi cleanup and suspends idle sleep during work. Progress uses grayscale differences at most once per second.
