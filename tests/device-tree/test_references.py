#!/usr/bin/env python3
"""Require known-reference equivalence without hiding changed arguments/raw data."""
import copy
import importlib.util
from pathlib import Path
import struct
import unittest

ROOT=Path(__file__).resolve().parents[2]
spec=importlib.util.spec_from_file_location('dt_audit',ROOT/'scripts/audit_boot_tree.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
def cells(*values):return struct.pack('>'+'I'*len(values),*values)

class References(unittest.TestCase):
    def pair(self,prop='clocks',left_value=None,right_value=None):
        left={'/':{},'/clock':{'phandle':cells(1),'#clock-cells':cells(1)},
              '/gpio':{'phandle':cells(2),'#gpio-cells':cells(2)},
              '/consumer':{prop:cells(1,7) if left_value is None else left_value}}
        right=copy.deepcopy(left)
        right['/clock']['phandle']=cells(101)
        right['/gpio']['phandle']=cells(102)
        right['/consumer'][prop]=cells(101,7) if right_value is None else right_value
        return left,right
    def changed(self,pair):
        return audit.semantic_compare(*pair)['changed_properties_excluding_chosen_and_symbol_metadata']
    def test_clock_reference_renumbering(self):
        self.assertNotIn('/consumer',self.changed(self.pair()))
    def test_clock_argument_change_remains_visible(self):
        self.assertIn('clocks',self.changed(self.pair(right_value=cells(101,8)))['/consumer'])
    def test_target_change_remains_visible(self):
        a,b=self.pair('pinctrl-0',cells(1),cells(102))
        self.assertIn('/consumer',self.changed((a,b)))
    def test_unknown_scalar_is_not_interpreted_as_phandle(self):
        self.assertIn('/consumer',self.changed(self.pair('vendor,raw-mode',cells(1),cells(101))))
    def test_missing_provider_arity_is_not_guessed(self):
        a,b=self.pair();del b['/clock']['#clock-cells']
        self.assertIn('/consumer',self.changed((a,b)))
    def test_truncated_arguments_remain_visible(self):
        self.assertIn('/consumer',self.changed(self.pair(right_value=cells(101))))
    def test_gpio_null_slot_and_arguments(self):
        self.assertNotIn('/consumer',self.changed(self.pair('reset-gpios',cells(0,2,9,1),cells(0,102,9,1))))
    def test_metadata_offsets_are_not_phandles(self):
        a,b=self.pair();a['/__local_fixups__/consumer']={'pinctrl-0':cells(1)}
        b['/__local_fixups__/consumer']={'pinctrl-0':cells(101)}
        self.assertIn('/__local_fixups__/consumer',self.changed((a,b)))
    def test_added_phandle_is_not_hidden(self):
        a,b=self.pair();del a['/clock']['phandle']
        self.assertIn('phandle',self.changed((a,b))['/clock'])

if __name__=='__main__':unittest.main()
