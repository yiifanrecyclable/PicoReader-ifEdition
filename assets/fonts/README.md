# 默认黑体资源 / Default sans font

`PicoSans-Medium.ttf` 是 Google Fonts 提供的标准 Noto Sans SC Medium 静态 TrueType 字体，仅更换文件名，字库字节未修改。覆盖30890个Unicode码位，含常用简繁汉字、扩展A及中英文标点；不是所有Unicode字符，例如部分扩展B汉字和emoji不在此字库中。保留原始字库和[完整OFL许可](OFL.txt)，没有使用用户提供的MoonStarsKai字体制作发行资源。

- 官方固定资源：[Google Fonts v41 TTF](https://fonts.gstatic.com/s/notosanssc/v41/k3kCo84MPvpLmixcA63oeAL7Iqp5IZJF9bmaG-3FnYw.ttf)
- 官方入口：[Google Fonts Noto Sans SC](https://fonts.google.com/noto/specimen/Noto+Sans+SC)
- 大小：10533840字节。
- SHA256：`09f17deea660cdd9cc2ba1e8b90adad18eabeca73b62b3c5f76b7ec3b61b33e0`。
- 原始OFL来自Google Fonts `ofl/notosanssc/OFL.txt`，Git blob `1c9f43281b8f216c5461fe9ac729afbade7724e4`；Copyright 2014–2021 Adobe，Reserved Font Name `Source`。

运行 `python3 tools/prepare_fonts.py --download --install-card` 校验/准备资源并放入项目TF卡。已有不同内容的同名字体不会被覆盖。后续发行包应把本目录的TTF和OFL放在TF卡的 `fonts/PicoSans-Medium.ttf` 与 `fonts/PicoSans-OFL.txt`，同时保留本说明。此资源不参与程序分区的链接。

内建 `main/assets/builtin.ttf` 通过 `tools/gen_builtin_font.py` 从同一标准字体生成，固定Medium字重，包含界面字符串、拉丁字符及标点。当前 IF 默认子集为1689个字符、347668字节，生成器预算上限768KiB；不再预装GB2312一级全集，以给微读、TLS及同步代码留出程序分区。保留横向字形、度量与kerning，移除渲染器不消费的竖排和替代字形数据。后续持续检查4MiB程序分区余量。无卡基础菜单由内建子集显示；广泛的图书/文档标题和正文覆盖依赖TF标准字体。实体字体性能仍待验收。

默认系统界面仍用内建黑体显示已有字符；遇到标题等动态文本缺字，自动使用卡上标准黑体。未选择正文自定义字体时，也默认使用卡上标准黑体。卡上标准字体缺失时仍能用内建界面字库显示基础界面；用户明确选择的系统/正文字体继续优先。字体按需读取，实际真机耗时与内存峰值仍须验收。
