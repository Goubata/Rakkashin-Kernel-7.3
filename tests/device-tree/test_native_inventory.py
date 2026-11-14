#!/usr/bin/env python3
"""Prevent stale firmware .ko files from entering new bridge-mode artifacts."""
from pathlib import Path
import sys
import tempfile
import unittest
from unittest.mock import patch

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
import native_module_inventory as inventory


class NativeInventory(unittest.TestCase):
    def prepare(self, root, order):
        out = root / 'out/native-gki'
        firmware = out / 'drivers/firmware/qcom'
        firmware.mkdir(parents=True)
        for name in ['qcom-scm.ko', 'qcom_pas.ko', 'qcom_tzmem.ko']:
            (firmware / name).write_bytes(b'fixture')
        (out / 'modules.order').write_text(order)
        return firmware

    def test_stale_separate_tzmem_is_excluded(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.prepare(root, 'drivers/firmware/qcom/qcom_pas.o\ndrivers/firmware/qcom/qcom-scm.o\n')
            with patch.object(inventory, 'ROOT', root):
                self.assertEqual([p.name for p in inventory.firmware_modules()], ['qcom_pas.ko', 'qcom-scm.ko'])

    def test_ko_order_format_is_supported(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.prepare(root, 'drivers/firmware/qcom/qcom_tzmem.ko\ndrivers/firmware/qcom/qcom-scm.ko\n')
            with patch.object(inventory, 'ROOT', root):
                self.assertEqual(len(inventory.firmware_modules()), 2)

    def test_missing_selected_module_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            fw = self.prepare(root, 'drivers/firmware/qcom/qcom-scm.o\n')
            (fw / 'qcom-scm.ko').unlink()
            with patch.object(inventory, 'ROOT', root), self.assertRaises(ValueError):
                inventory.firmware_modules()

    def test_missing_scm_selection_fails(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            self.prepare(root, 'drivers/firmware/qcom/qcom_tzmem.o\n')
            with patch.object(inventory, 'ROOT', root), self.assertRaises(ValueError):
                inventory.firmware_modules()


if __name__ == '__main__':
    unittest.main()
