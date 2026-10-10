#!/usr/bin/env python3
"""刷机包分区边界回归测试。/ Regression tests for flashing partition boundaries."""
# 2026-10-05 新增：独立 Pico 阅读固件的容量与损坏分区回归测试。
# Added 2026-10-05: capacity and damaged-partition regression tests for the independent reader.

import contextlib
import hashlib
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest

from verify_flash_bundle import application_capacity, check


def partition(kind, subtype, offset, size):
    return struct.pack("<HBBII16sI", 0x50AA, kind, subtype, offset, size, b"test", 0)


def table(capacity=0x400000, storage=0x410000):
    entries = partition(0, 0, 0x10000, capacity) + partition(1, 129, storage, 0x1000)
    return entries + b"\xeb\xeb" + b"\xff" * 14 + hashlib.md5(entries).digest() + b"\xff" * 32


class FlashBoundaryTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.folder = Path(self.temporary.name)
        source = Path(__file__).resolve().parents[1] / "flash"
        (self.folder / "manifest.json").write_bytes((source / "manifest.json").read_bytes())
        for name in ("index.html", "vendor/web/install-button.js", "licenses/CROSSPOINT-MIT.txt",
                     "licenses/LUCIDE-ISC.txt", "bootloader.bin"):
            target = self.folder / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(b"fixture")
        (self.folder / "partitions.bin").write_bytes(table())

    def verify(self, length):
        with (self.folder / "firmware.bin").open("wb") as firmware:
            firmware.truncate(length)
        with contextlib.redirect_stdout(io.StringIO()):
            check(self.folder)

    def test_exact_factory_capacity_accepted(self):
        self.verify(0x400000)

    def test_one_byte_into_storage_rejected(self):
        with self.assertRaisesRegex(AssertionError, "partition capacity"):
            self.verify(0x400001)

    def test_smaller_bundled_partition_controls_limit(self):
        (self.folder / "partitions.bin").write_bytes(table(0x200000, 0x210000))
        self.verify(0x200000)
        with self.assertRaisesRegex(AssertionError, "partition capacity"):
            self.verify(0x200001)

    def test_empty_firmware_rejected(self):
        with self.assertRaisesRegex(AssertionError, "invalid size"):
            self.verify(0)

    def test_wrong_manifest_offset_rejected(self):
        manifest = json.loads((self.folder / "manifest.json").read_text())
        manifest["builds"][0]["parts"][2]["offset"] = 0x20000
        (self.folder / "manifest.json").write_text(json.dumps(manifest))
        with self.assertRaisesRegex(AssertionError, "wrong offset"):
            self.verify(100)

    def test_invalid_tables_rejected(self):
        cases = {
            "truncated": table()[:-1],
            "missing factory": partition(1, 129, 0x410000, 0x1000),
            "duplicate factory": partition(0, 0, 0x10000, 0x1000) * 2,
            "wrong factory offset": partition(0, 0, 0x20000, 0x1000),
            "zero capacity": partition(0, 0, 0x10000, 0),
            "overlap": table(0x400001),
            "outside flash": partition(0, 0, 0x10000, 0x1000000),
            "bad MD5": table()[:64] + b"\xeb\xeb" + b"\xff" * 14 + b"\0" * 16,
            "bad magic": b"\0" * 32,
        }
        for name, data in cases.items():
            with self.subTest(name=name), self.assertRaises(AssertionError):
                application_capacity(data)

    def test_baseline_bundle_accepted(self):
        with contextlib.redirect_stdout(io.StringIO()):
            check(Path(__file__).resolve().parents[1] / "flash")


if __name__ == "__main__":
    unittest.main()
