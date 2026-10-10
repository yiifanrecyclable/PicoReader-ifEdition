# 统一文字输入 / Shared text input

首页格言、签名、坚果云文件夹/账号/应用密码、文件重命名、图书和文档搜索、书名编辑及 WiFi 密码共用同一组件。输入期间隐藏主导航。候选栏、四排键盘和最下排功能键固定在屏幕底部；末排底边为1200，距1216像素屏幕底边16像素，不随入口或系统字号改变。

Home quote, signature, Nutstore folder/account/application password, file rename, book/document search, title editing and WiFi password share one component. Main navigation is hidden during editing. Candidates and four key rows are bottom-anchored; the action row ends at y1200, 16px above the 1216px screen edge, independently of entry point or system font scaling.

- 点文字定位光标，使用左右箭头微调；支持在中间插入和删除，按完整UTF-8字符移动。
- 中文/英文与数字/符号切换；大写按钮切换英文大小写。中文候选未确认时，切模式或提交会提示先选字，也可点“输入原文”保留拼音。
- 空格在有候选时选第一项，否则插入空格；没有候选的拼音可作为原文输入。
- 候选左右按钮翻页，也支持在候选栏左右滑动，最后一页不能继续翻空页。
- 数字/符号有两页，使用候选栏左右按钮切换；覆盖全部可打印ASCII标点。中文输入场景还提供常用中文标点，账号和密码只提供英文符号。
- 密码默认隐藏，可点“显示密码”；退出时清除输入缓冲。输入不参与词组学习，不写入日志。
- 搜索默认英文，可直接输入首字母/拼音，也可切中文选字。取消不改变筛选条件，应用仍只改变所属图书/文档页面。
- 重命名锁定原扩展名，文件名规则及同名冲突仍由文件服务检查。失败保留编辑内容，支持修正后重试。
- 设置字段完成后沿用已有保存规则；坚果云配置随设置备份保存。取消不提交。

Tap text to position the UTF-8 cursor; use arrows for precise positioning and insert/delete in the middle. Switch Chinese/English or numbers/symbols; uppercase toggles English case. Resolve pending pinyin before switching/submitting, or use the literal-input button. Space chooses the first candidate when available. Candidate paging is bounded; horizontal swipes apply only to the candidate strip. Symbol paging covers all printable ASCII punctuation, with Chinese punctuation available only in Chinese-enabled fields. Secrets are masked and can be revealed; buffers are cleared on exit, without learning/logging. Search retains direct pinyin/initial matching and independent book/document queries. Rename protects extensions and preserves drafts on validation errors. Page-specific persistence/backup rules remain in effect.

## TF词组资源 / TF phrase resource

将[词库资源](../assets/ime/)里的`pinyin.ifdict`及许可放到TF卡`system/ime/`目录。标准界面字体仍放在`fonts/`；推荐保留项目提供的完整`PicoSans-Medium.ttf`，以显示完整中文候选。

Copy `pinyin.ifdict` and its license from [resources](../assets/ime/) to TF `/system/ime/`. Keep the full `PicoSans-Medium.ttf` in `/fonts/` for Chinese candidate coverage.

当前词库来自固定的`pypinyin==0.55.0` MIT数据，包含46,111个两至四字词组，共2,951,120字节。连续输入无声调全拼，`v`表示ü；例如`yuedu`可以选“阅读”。设备常用词优先，其余是确定性排列，不提供整句预测、上下文纠错或自学习。词库无需联网，也不嵌入固件。

The pinned pypinyin 0.55.0 MIT pack contains 46,111 phrases of 2–4 Chinese characters, using 2,951,120 bytes. Type continuous toneless full pinyin (`v` for ü), e.g. `yuedu` → 阅读. Device vocabulary is preferred; remaining order is deterministic, without sentence prediction, contextual correction or learning. The pack is offline and outside the app image.

固件对固定64字节记录二分查找，仅读取需要的记录，不把约3MB词库整体载入RAM，也不持有跨输入事件的TF文件句柄。词库缺失、无法读取或遇到不合法格式时使用内建单字候选。内建拼音表也供书名搜索使用。候选范围仍受可用字体覆盖影响；真机TF访问延迟需实测。

Firmware binary-searches 64-byte records without loading the whole pack or retaining TF handles between input events. Missing/unreadable/invalid packs fall back to built-in character candidates, sharing the search dictionary. Available fonts still determine glyph coverage; real-card latency requires hardware testing.

生成工具不安装或执行第三方代码，校验固定wheel的SHA-256后读取字典：

Generation validates the pinned wheel digest and reads its data without installing/executing third-party code:

```sh
python -m pip download --no-deps pypinyin==0.55.0 -d build/search-deps
python tools/generate_input_dictionary.py build/search-deps/pypinyin-0.55.0-py2.py3-none-any.whl assets/ime/pinyin.ifdict
```

## 验证 / Verification

测试只使用临时TF目录及私有NVS，不能对真实用户TF卡运行破坏性夹具。

Tests use temporary TF directories/private NVS, never destructive fixtures on the real user card.

```sh
bash tools/simulator/build.sh
python tools/test_text_input.py
python tools/test_input_pages.py
python tools/test_content_library.py
python tools/settings_scroll_host_test.py
python tools/test_if_simulator.py
```

首页banner长按进入「首页文字」，可查看与首页同尺寸的实际效果，修改第三行文字、选择该行独立TF字体或调整字号与排版。字号下方提供左对齐、右对齐、居中、两端对齐，默认居中；选择后立即更新实际预览并保存。前两行始终居中，视觉行间距14px。两端对齐逐行铺满，中文均匀扩字距，英文有空格时扩词距，单个字符居中。文字和字体编辑完成后回到预览页，「完成」返回首页。字号滑块为48–120px、2px步进，默认96px；拖动时更新滑块和数值，松手后生成实际预览并保存有效字号。字体只影响banner文字，不改变系统界面和阅读字体；文件缺失/无法加载时回退。文字、字体、字号和排版偏好随设置的阅读资料备份。

首页文字按所选字体与当前字号的实际字宽自动换行，最多两行。保存、换字体或调整字号时检查行数和banner可用高度；超限不会覆盖已存文字、字体或字号，输入页面保留内容供缩短。旧版过长格言保留原文，显示最多两行并省略，重新编辑时须满足新限制。
