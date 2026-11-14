#!/usr/bin/env python3
"""Check recovered memory references without baking numeric phandles into them."""
import copy
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from audit_boot_tree import fdt
from reconstruct_memory_dt import POOL, HEAP, comparable


class MemoryReferences(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.nodes, _ = fdt((ROOT / 'artifacts/dt-input-audit/stock-merged.dtb').read_bytes())

    def test_pool_reference_can_be_renumbered(self):
        nodes = copy.deepcopy(self.nodes)
        phandles = {int.from_bytes(p['phandle'], 'big') for p in nodes.values() if 'phandle' in p}
        new = struct.pack('>I', max(phandles) + 1)
        nodes[POOL]['phandle'] = new
        nodes[HEAP]['memory-region'] = new
        self.assertEqual(comparable(nodes, HEAP), comparable(self.nodes, HEAP))

    def test_heap_cannot_refer_to_other_pool(self):
        nodes = copy.deepcopy(self.nodes)
        other = next(p for p, props in nodes.items() if p != POOL and 'phandle' in props)
        nodes[HEAP]['memory-region'] = nodes[other]['phandle']
        with self.assertRaises(ValueError):
            comparable(nodes, HEAP)

    def test_pool_size_change_stays_visible(self):
        nodes = copy.deepcopy(self.nodes)
        nodes[POOL]['size'] = (int.from_bytes(nodes[POOL]['size'], 'big') + 4096).to_bytes(8, 'big')
        self.assertNotEqual(comparable(nodes, POOL), comparable(self.nodes, POOL))

    def test_dynamic_pool_cannot_gain_fixed_address(self):
        nodes = copy.deepcopy(self.nodes)
        nodes[POOL]['reg'] = bytes(16)
        with self.assertRaises(ValueError):
            comparable(nodes, POOL)


if __name__ == '__main__':
    unittest.main()
