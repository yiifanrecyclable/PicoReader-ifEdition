# IF Edition offline Pinyin phrases

Copy `pinyin.ifdict`, `pinyin.LICENSE.txt` and `pinyin.README.txt` together to TF `/system/ime/`. The pack is separate from the firmware image. Keep the standard system font in TF `/fonts/` for Chinese glyph coverage. See [input behavior and regeneration](../../docs/TEXT_INPUT.md).

将这三个文件一起放到TF卡`system/ime/`。词库不占固件分区，标准字体放在`fonts/`。详见[输入操作和再生成](../../docs/TEXT_INPUT.md)。

- Data: pypinyin **0.55.0**, original MIT license retained in `pinyin.LICENSE.txt`.
- Source: <https://github.com/mozillazg/python-pinyin>, official wheel `phrases_dict.json`.
- Wheel SHA-256: `d53b1e8ad2cdb815fb2cb604ed3123372f5a28c6f447571244aca36fc62a286f`.
- Pack SHA-256: `af73799d248f1d55351eae5a53e9d6680d91861532f2bb8740327ab10a1bf530`.
- 46,111 phrases; **2,951,120 bytes**; 2–4 Han characters per phrase.
- Format: 16-byte `IFPY001\0` header (LE count, four reserved zero bytes), followed by 64-byte records (33-byte NUL-padded lowercase full-pinyin key, 31-byte NUL-padded UTF-8 word).
- Records sorted by pinyin key. Preferred device vocabulary first for equal keys, then shorter phrase and Unicode order. This is not a frequency-ranked sentence engine.
- Missing/unreadable/invalid packs retain built-in character input. The runtime uses bounded record reads without a whole-file allocation.
