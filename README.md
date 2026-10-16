# if-ink OS

基于 [Pico Reader 0.3.3-rc83](https://github.com/wegooo-cell/read-pico-reader) 的独立衍生版本，增加坚果云同步、Markdown 阅读与界面调整。仅适用 Pico RDP-G01-W。

公开下载版本：**Pico Reader IF Edition 0.3.3-rc83-if.8**。if-ink OS v0.1.0 正在开发，尚未发布。

设备在线升级使用[独立稳定通道](ota/README.md)，首个正式版本验收发布后再开放。

[下载与手动刷机教程](https://yiifanrecyclable.github.io/if-ink-OS/) · [GitHub 发布版本](https://github.com/yiifanrecyclable/if-ink-OS/releases/tag/0.3.3-rc83-if.8) · [中文安装说明](docs/安装说明.md)

首次安装推荐使用电脑Chrome/Edge打开下载页在线刷机，无需安装Python或esptool；USB手动安装包作为备用。已安装IF后可使用TF更新包。第一次准备TF字体与词库资源，后续更新无需重装。用户已确认本地阅读、坚果云文档拉取保存与 if.8 表格显示在实机可用。

源码、固定依赖与实际构建配置均在本仓库。[构建与重新链接说明](BUILDING.md) · [对应源码快照](docs/Pico-IF-0.3.3-rc83-if.8-source.zip)。插件与手动放入的MD/TXT/EPUB均可从配置的坚果云文件夹拉取；Obsidian端插件本次不在固件下载页提供，可联系作者获取。

感谢原作者：[Pico Reader 主页](https://wegooo-cell.github.io/read-pico-reader/) · [原项目源码](https://github.com/wegooo-cell/read-pico-reader)。IF不是原作者或MindReset官方发布。主体采用[Apache-2.0](LICENSE)，epdiy采用LGPL-3.0-or-later，其他组件许可与上游版权见[第三方声明](THIRD_PARTY_NOTICES.md)。
