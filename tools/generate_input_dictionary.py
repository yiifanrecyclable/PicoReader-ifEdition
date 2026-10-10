#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 固定MIT数据包生成TF离线词组；不导入第三方代码。/ Generate TF phrases from pinned MIT data, without executing it.
import argparse, hashlib, json, struct, zipfile
from pathlib import Path
from generate_search_table import VERSION, WHEEL_SHA256, normalize


def generate(wheel: Path, output: Path):
    if hashlib.sha256(wheel.read_bytes()).hexdigest() != WHEEL_SHA256:
        raise ValueError('Pinned pypinyin wheel checksum mismatch')
    with zipfile.ZipFile(wheel) as source:
        phrases = json.loads(source.read('pypinyin/phrases_dict.json'))
        license_text = source.read(f'pypinyin-{VERSION}.dist-info/licenses/LICENSE.txt')
    words = set()
    for word, readings in phrases.items():
        if not 2 <= len(word) <= 4 or not all(0x4e00 <= ord(c) <= 0x9fff for c in word):
            continue
        if len(readings) != len(word) or any(not syllables for syllables in readings):
            continue
        spelling = ''.join(normalize(syllables[0]) for syllables in readings)
        if len(spelling) > 32:
            continue
        words.add((spelling, word))
    # 常用设备词优先，其余保持确定性顺序；不宣称整句预测或语料词频。/ Prefer device vocabulary, otherwise deterministic order; no sentence prediction/frequency claim.
    preferred = {'阅读', '读书', '图书', '文档', '书籍', '书架', '文件', '文件夹', '同步', '内容', '设置', '名称', '搜索', '管理', '时间', '日期', '学习', '笔记', '工作', '生活'}
    ordered = sorted(words, key=lambda item: (item[0], item[1] not in preferred, len(item[1]), item[1]))
    assert len(ordered) <= 100000
    output.parent.mkdir(parents=True, exist_ok=True)
    data = bytearray(b'IFPY001\0' + struct.pack('<II', len(ordered), 0))
    for spelling, word in ordered:
        data.extend(spelling.encode().ljust(33, b'\0'))
        data.extend(word.encode().ljust(31, b'\0'))
    output.write_bytes(data)
    output.with_suffix('.LICENSE.txt').write_bytes(license_text)
    output.with_suffix('.README.txt').write_text(
        f'IF Edition offline Pinyin phrase dictionary\nSource: pypinyin {VERSION}, MIT\n'
        f'https://github.com/mozillazg/python-pinyin\nWheel SHA256: {WHEEL_SHA256}\n'
        f'{len(ordered)} phrases, {len(data)} bytes.\n'
        'Copy pinyin.ifdict and its license to TF card /system/ime/.\n'
        '2–4 character exact full-pinyin phrases; v represents ü. No sentence prediction.\n'
        'Missing dictionary: built-in character input remains available.\n', encoding='utf-8')
    print(f'{len(ordered)} phrases; {len(data)} bytes; sha256={hashlib.sha256(data).hexdigest()}')


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('wheel', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    generate(args.wheel, args.output)
