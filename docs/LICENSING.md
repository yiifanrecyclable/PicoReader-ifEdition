# 二次开发与发布许可 / Derivative development and distribution

2026-10-05 新增：独立 Pico 阅读固件的衍生发布说明；本文件不改变上游或第三方许可。
Added 2026-10-05: distribution guidance for the independent Pico reader; upstream and third-party licenses remain unchanged.

本项目在 Pico Reader `0.3.3-rc58` 源码基线上开发，是独立阅读固件，不能以 MindReset 官方发布的身份宣传。设备名称继续使用 Pico。本文说明发布材料与审查要求，不替换许可证原文，也不是无条件的法律合规保证。各文件及组件继续适用各自许可；主体 Apache-2.0 不覆盖第三方许可。

This independent firmware develops from the Pico Reader `0.3.3-rc58` source baseline. It must retain its own project identity and acknowledge its origin. This document describes distribution requirements, not a legal guarantee or a replacement for the applicable licenses.

## v0.1.1 发行核查 / Release verification

2026-10-16：本版提供对应应用源码、修改后的 epdiy、固定依赖、实际 `sdkconfig.release` 和 `BUILDING.md`。USB、TF、资源及许可包包含来源和适用许可；刷机页同时提供同版源码。源码 ZIP 的独立解压副本中对 epdiy 作接口兼容改动后，按公开配置完成 ESP-IDF 重新编译链接。发布应用、各包成员及网页清单的摘要均已核对；包中不含用户 TF 卡内容或凭据。下方早期未完成项目为历史基线核查记录，应结合当前版本材料阅读。

This release includes the matching application/library source, pinned dependencies, actual configuration, build/relink instructions and license texts. An independently extracted source archive was rebuilt after a compatible epdiy modification. Image, package-member and web-manifest checksums were verified; user card content and credentials are excluded. Earlier outstanding items below document the initial baseline.

## 主体代码 / Application code

[Apache-2.0 §2、§4、§6](https://www.apache.org/licenses/LICENSE-2.0)允许在满足条件时修改并发布源码或二进制。随分发物提供 [LICENSE](../LICENSE)，保留原版权、许可及适用署名；修改过的文件携带显著修改声明。Git 历史和版本日志可帮助追溯，但不能替代文件中的修改声明。原项目有适用 NOTICE 时保留其声明；[THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md)也随交付保存。可为新增内容补充真实作者声明，不能覆盖原作者。许可不授予以原作者商标为新项目背书的权利。

Apache-2.0 permits derivatives and source/binary distribution subject to its conditions. Include the license, preserve applicable notices, and mark modified files prominently. Keep attribution and independent project identity; do not imply upstream endorsement. New copyright statements must not replace the original ones.

## epdiy 与重新链接 / epdiy and relinking

`components/epdiy` 是 LGPL-3.0-or-later 的修改版，原声明与裁剪说明见其 [LICENSE](../components/epdiy/LICENSE)。当前 ESP-IDF 组件构建把它编入固件；正式发布应核对链接 map，并按 [LGPLv3 §4(d)(0)](https://raw.githubusercontent.com/vroland/epdiy/master/LICENSE)准备静态链接交付，而不是依赖动态共享库例外。条文全文在 [LGPLv3](../licenses/LGPL-3.0.txt)和 [GPLv3](../licenses/GPL-3.0.txt)。

采用的交付安排：每个正式版本同时提供实际发行 tag 对应的修改后 epdiy 源码及构建资料（Minimal Corresponding Source），以及能够替换该库后重建固件的应用源码、资源、脚本和链接配置（Corresponding Application Code）。后者可用完整可重建源码承担；若源码方案无法完成重新链接，须补充可重链接对象文件、库及链接命令。保留库许可和修改说明，显著说明固件使用 epdiy；条款不得禁止用户修改库或为调试该修改而逆向。固件若运行时展示版权，应加入对应库版权及许可入口。

The static-link distribution plan follows LGPLv3 §4(d)(0): supply the modified library source and Corresponding Application Code sufficient to rebuild with a modified library. Complete rebuildable application source may serve this purpose; otherwise provide relinkable objects and commands. Preserve notices, both GNU license texts, and the user's library-modification and debugging rights.

按 [GPLv3 §6(d)](https://ftp.gnu.org/gnu/Licenses/gpl-3.0.txt)，二进制下载与刷机入口旁提供同版本材料的明确免费获取入口，并维持其可用性。仅链接随时变化的 `main` 不足以说明对应关系。固定 IDF、工具链、依赖来源/版本及配置；不能笼统把所有 SDK 组件或闭源静态库视作 System Libraries，需逐项判断是否需提供重新链接材料及其分发权利。

Download and flashing pages must identify the matching freely accessible source/relinking materials, including fixed dependencies and build configuration. Check SDK components and binary libraries individually; do not assume all qualify as System Libraries.

当 LGPLv3 §4(e) 所引用的 GPLv3 §6 User Product 交易条件适用时，还须交付安装修改后固件所需信息。至少提供从 `build/flash_args` 取得地址的刷写及恢复步骤；若存在安装授权限制，单独审查所需材料。不能仅因发布网页刷机就推定所有交易条件都已满足。

Provide Installation Information when the referenced User Product conditions apply. Document flashing from generated `flash_args` and recovery; review any installation authorization restrictions separately.

发布验收应从干净目录取得发行材料，替换一处保持接口兼容的 epdiy 实现，按公开说明成功重新编译和链接；在隔离副本进行，不为验收改动主工作区硬件驱动。保存结果后才能认定重新链接安排已落实。这不自动表示真机或全部许可已验收。

Validate the published materials in a clean isolated directory by rebuilding with an interface-compatible library modification. Source availability alone does not prove that this works.

## 已识别的资源 / Identified material

以下是已识别材料，不是整个 SDK 的完整软件物料清单。最终以发行构建实际输入和各版本原许可为准。
This inventory is not a complete SDK bill of materials; use the actual release inputs and version-specific licenses.

| 材料 / Material | 许可与交付 / License and delivery |
| --- | --- |
| MindReset BSP、主体代码及 TinyUSB 适配层 | Apache-2.0；保留 [主体许可](../LICENSE)、文件头和[适配层许可](../components/espressif__esp_tinyusb/LICENSE)。 |
| epdiy 修改版 | LGPL-3.0-or-later；源码、修改说明、GPL/LGPL 全文和上述重新链接安排。 |
| md4c 0.5.2 解析器 | MIT，Copyright © 2016–2024 Martin Mitáš；上游 [mity/md4c](https://github.com/mity/md4c)，固定 `release-0.5.2` commit `729e6b8b320caa96328968ab27d7db2235e4fb47`。保留[原许可](../components/md4c/LICENSE.md)、源文件头及[来源/本地修改说明](../components/md4c/UPSTREAM.md)。解析器的本地修正仍适用 MIT，不改变主体许可。 |
| cJSON 1.7.19 / Espressif 1.7.19~2 | MIT；[声明](../licenses/CJSON-MIT.txt)、[宿主来源](../tools/simulator/vendor/cjson/README.md)，固件版本/hash见锁文件。 |
| Expat / Espressif 2.8.4 | MIT；[原声明](../licenses/EXPAT-MIT.txt)，固定版本/hash见锁文件。 |
| CrossPoint 图标设计 | MIT；保留[随库声明](../flash/licenses/CROSSPOINT-MIT.txt)，一手文本见 [CrossPoint LICENSE](https://raw.githubusercontent.com/crosspoint-reader/crosspoint-reader/master/LICENSE)。 |
| Lucide 及 Feather 衍生图标 | ISC / MIT；保留[全部图标声明](../flash/licenses/LUCIDE-ISC.txt)，见 [Lucide 官方许可](https://lucide.dev/license)。 |
| 内建 Noto Sans SC Medium 子集 | OFL-1.1；保留[字体声明](../main/assets/OFL-Noto.txt)和[OFL 条件](https://openfontlicense.org/open-font-license-official-text/)。子集也属于修改版；核对实际字体来源、版权和 Reserved Font Name，名称限制以来源版本为准。不能把字体改标 Apache，也不随刷机站点提供来源不明的额外字体包。 |
| stb_truetype | 文件内提供 MIT / public-domain 两种选择；发布材料保留[原文件声明](../main/font/stb_truetype.h)。一手文件见 [stb](https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h)。若采用 MIT 路径，二进制许可包也需完整声明。 |
| pypinyin 字典数据 | MIT；保留[字典声明](../components/read_pico_search/LICENSE.pypinyin)。 |
| ESP Web Tools | Apache-2.0；保留[本地许可](../flash/vendor/LICENSE)，一手文本见 [ESP Web Tools LICENSE](https://raw.githubusercontent.com/esphome/esp-web-tools/main/LICENSE)。另核对打包 JS 的传递依赖许可和确切版本。 |
| ESP-IDF 及 managed components | IDF 主许可为 [Apache-2.0](https://raw.githubusercontent.com/espressif/esp-idf/v6.1/LICENSE)，内部组件和传递依赖可有不同许可；须从发行构建目录收集原声明。 |

## 尚需落实的发行材料 / Outstanding release materials

- 当前 `stage_public_flash.py` 只复制部分许可，不包含此文档、`THIRD_PARTY_NOTICES.md`、epdiy 修改说明、stb 声明及重新链接材料。正式发布需补齐可下载包和旁边的同版本入口，不能把现有 staging 成功视为许可验收。
- `main/idf_component.yml` / `dependencies.lock` 涉及 qrcode、esp_jpeg、libpng、zlib、cJSON、TinyUSB；本机已获取锁定版本的 `managed_components/`；cJSON/Expat声明已保留，最终发行仍须核对其余依赖和SDK的实际输入。字体渲染库也未列入现有第三方清单。构建完成后核对使用版本、原版权及 libpng/zlib 等嵌套材料，按实际许可补齐。
- 固件依赖的 IDF/芯片 SDK 中闭源库和 ROM 调用、浏览器 flasher 的传递依赖尚须盘点。md4c 已固定为 0.5.2 并保留 MIT 原文，正式二进制/刷机站点仍须随包提供该声明；新增其他解析器时同样固定来源与版本并收集许可。
- 默认字体来源与OFL已核实并固定：[字体包来源/校验](../assets/fonts/README.md)。内建常用字子集从同一标准Noto Sans SC Medium生成，保留Adobe/Source原声明；后续发布须同时打包卡上标准TTF与OFL，不能只提供固件后宣称所有标题均已补全。
- 发行版本、源码 tag、构建配置、生成资源、三段二进制及校验摘要须对应。现存 rc58 包的应用描述版本为 `bf512d8-dirty`，不能据 manifest 名称就认定已满足可复建交付要求；保留基线来源记录，并为新版本重建、验证对应关系。

Before release, complete the staged notices and matching rebuild materials; audit actual managed/SDK/browser dependencies; verify font provenance and naming; and match source, configuration, resources and binary hashes. The existing rc58 manifest label alone does not establish reproducibility.

完成这些工程检查后仍需根据最终分发形式审阅适用义务。本地开发可以继续；正式二进制发布须在版本确认、产品验收及发行材料核查完成后进行。
Review obligations against the final distribution form. Local development can continue; binary publication follows version confirmation, product acceptance and material verification.
