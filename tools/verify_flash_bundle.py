#!/usr/bin/env python3
"""Validate the small, public Read Pico web-flashing bundle."""
# 2026-10-05 修改（独立 Pico 阅读固件）：读取分区容量并检查范围、重叠和摘要；保留原校验。
# Modified 2026-10-05 for the independent Pico reader: validate partition capacity,
# ranges, overlap and digest while retaining the original bundle checks.

from __future__ import annotations

import hashlib
import json
import struct
import sys
from pathlib import Path


EXPECTED = {
    "bootloader.bin": (0x0, 0x8000),
    "partitions.bin": (0x8000, 0x10000),
    "firmware.bin": (0x10000, 0x410000),
}


def application_capacity(data: bytes) -> int:
    """读取实际 factory 分区容量。/ Read the actual factory partition capacity."""
    assert data and len(data) % 32 == 0, "truncated partition table"
    entries = []
    for position in range(0, len(data), 32):
        record = data[position:position + 32]
        magic = struct.unpack_from("<H", record)[0]
        if magic == 0xFFFF:
            assert all(value == 0xFF for value in data[position:]), "invalid partition padding"
            break
        if magic == 0xEBEB:
            assert record[:16] == b"\xeb\xeb" + b"\xff" * 14, "invalid partition MD5 record"
            assert record[16:] == hashlib.md5(data[:position]).digest(), "partition MD5 mismatch"
            assert all(value == 0xFF for value in data[position + 32:]), "data after partition MD5"
            break
        assert magic == 0x50AA, "invalid partition entry"
        _, kind, subtype, offset, size, _, _ = struct.unpack("<HBBII16sI", record)
        assert size > 0 and offset >= 0x9000 and offset + size <= 0x1000000, "invalid partition range"
        entries.append((kind, subtype, offset, size))
    factories = [entry for entry in entries if entry[:2] == (0, 0)]
    assert len(factories) == 1, "expected one factory application partition"
    _, _, start, capacity = factories[0]
    assert start == EXPECTED["firmware.bin"][0], "factory partition disagrees with flash offset"
    for previous, following in zip(sorted(entries, key=lambda entry: entry[2]),
                                   sorted(entries, key=lambda entry: entry[2])[1:]):
        assert previous[2] + previous[3] <= following[2], "overlapping partitions"
    return capacity


def check(folder: Path) -> None:
    manifest = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
    assert manifest.get("new_install_prompt_erase") is False, "flasher must preserve user data"
    builds = manifest.get("builds")
    assert isinstance(builds, list) and len(builds) == 1
    assert builds[0].get("chipFamily") == "ESP32-S3"
    parts = builds[0].get("parts")
    assert isinstance(parts, list) and len(parts) == len(EXPECTED)
    capacity = application_capacity((folder / "partitions.bin").read_bytes())
    seen: set[str] = set()
    for part in parts:
        name = part.get("path")
        assert name in EXPECTED and name not in seen, f"unexpected flash part: {name}"
        seen.add(name)
        start, end = EXPECTED[name]
        assert part.get("offset") == start, f"wrong offset for {name}"
        data = (folder / name).read_bytes()
        limit = capacity if name == "firmware.bin" else end - start
        assert data and len(data) <= limit, f"invalid size for {name}: exceeds partition capacity {limit}"
        print(f"{name}: {len(data)} bytes, SHA256 {hashlib.sha256(data).hexdigest()}")
    assert seen == set(EXPECTED)
    assert (folder / "index.html").is_file()
    assert (folder / "vendor/web/install-button.js").is_file()
    assert (folder / "licenses/CROSSPOINT-MIT.txt").is_file()
    assert (folder / "licenses/LUCIDE-ISC.txt").is_file()


if __name__ == "__main__":
    check(Path(sys.argv[1]) if len(sys.argv) > 1 else Path("flash"))
