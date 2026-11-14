#!/usr/bin/env python3
"""Recover the saved AVR GPU table, without inventing frequency/voltage values."""
import argparse
import hashlib
import json
from pathlib import Path
import re
from audit_boot_tree import fdt

ROOT = Path(__file__).resolve().parents[1]
TABLE = '/soc/qcom,kgsl-3d0@3d00000/qcom,gpu-pwrlevel-bins/qcom,gpu-pwrlevels-0'
TARGET = ROOT / 'ports/device-tree/recovered/avr-saved-gpu-pwrlevels.dtso'


def saved_table():
    saved = ROOT / 'artifacts/dt-input-audit/stock-merged.dtb'
    nodes, _ = fdt(saved.read_bytes())
    table = {p: props for p, props in nodes.items() if p == TABLE or p.startswith(TABLE + '/')}
    parent = {'qcom,sku-codes', 'qcom,initial-pwrlevel', '#size-cells', '#address-cells'}
    child = {'reg', 'qcom,gpu-freq', 'qcom,level', 'qcom,acd-level', 'qcom,bus-freq', 'qcom,bus-min', 'qcom,bus-max'}
    limit_header = ROOT / 'ports/kgsl/kgsl_pwrctrl.h'
    limit = int(re.search(r'^#define\s+KGSL_MAX_PWRLEVELS\s+(\d+)', limit_header.read_text(), re.M)[1])
    levels = {}
    for path, props in table.items():
        if set(props) != (parent if path == TABLE else child):
            raise ValueError('Unexpected table properties; review before copying: ' + path)
        if any(len(value) != 4 for value in props.values()):
            raise ValueError('Only the observed scalar table contract is supported')
        values = {key: int.from_bytes(value, 'big') for key, value in props.items()}
        if path == TABLE:
            continue
        index = values['reg']
        if path != TABLE + '/qcom,gpu-pwrlevel@' + str(index) or index >= limit:
            raise ValueError('Saved level exceeds migrated driver capacity or node/index differs')
        if not 0 < values['qcom,bus-min'] <= values['qcom,bus-freq'] <= values['qcom,bus-max']:
            raise ValueError('Invalid saved bus floor/target/ceiling')
        levels[index] = values
    if sorted(levels) != list(range(len(levels))) or not levels:
        raise ValueError('Non-contiguous table')
    if int.from_bytes(table[TABLE]['qcom,initial-pwrlevel'], 'big') not in levels:
        raise ValueError('Invalid initial level')
    frequencies = [levels[i]['qcom,gpu-freq'] for i in sorted(levels)]
    if any(a <= b for a, b in zip(frequencies, frequencies[1:])) or frequencies[-1] <= 0:
        raise ValueError('Saved frequencies must be positive and strictly decreasing')
    return saved, table, levels, limit_header, limit


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--verify-merged', type=Path)
    args = parser.parse_args()
    saved, table, levels, header, limit = saved_table()
    running = ROOT / 'device-baseline/running.dtb'
    live_nodes, _ = fdt(running.read_bytes())
    live_table = {p: props for p, props in live_nodes.items() if p == TABLE or p.startswith(TABLE + '/')}
    if live_table != table:
        raise ValueError('Saved running GPU table differs from static image; review before copying')
    lines = ['/* SPDX-License-Identifier: GPL-2.0-only */',
             '/* Recovered from the saved AVR static DT, not a new tuning proposal.',
             ' * No hardware voltage/firmware or 7.3 runtime validation is implied. */',
             '/dts-v1/;', '/plugin/;', '', '/ {', '\tfragment@0 {',
             '\t\ttarget-path = "' + TABLE + '";', '\t\t__overlay__ {']
    for key, value in sorted(table[TABLE].items()):
        lines.append('\t\t\t' + key + ' = <0x' + value.hex() + '>;')
    for index in sorted(levels):
        lines.append('\t\t\tqcom,gpu-pwrlevel@' + str(index) + ' {')
        for key, value in sorted(levels[index].items()):
            lines.append('\t\t\t\t' + key + ' = <0x' + format(value, '08x') + '>;')
        lines.append('\t\t\t};')
    content = '\n'.join(lines + ['\t\t};', '\t};', '};', ''])
    if args.write:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(content)
    if TARGET.read_text() != content:
        raise ValueError('Recovered source differs from saved table')
    report = {'scope': 'exact saved GPU table reproduction and driver index capacity, not voltage safety, firmware or GPU execution',
              'saved_dtb_sha256': hashlib.sha256(saved.read_bytes()).hexdigest(),
              'saved_running_dtb_sha256': hashlib.sha256(running.read_bytes()).hexdigest(),
              'static_and_saved_running_table_match': True,
              'output': str(TARGET.relative_to(ROOT)), 'output_sha256': hashlib.sha256(TARGET.read_bytes()).hexdigest(),
              'driver_capacity_header_sha256': hashlib.sha256(header.read_bytes()).hexdigest(),
              'driver_max_levels': limit, 'level_count': len(levels),
              'levels': levels, 'runtime_verified': False, 'packaging_allowed': False}
    if args.verify_merged:
        nodes, _ = fdt(args.verify_merged.read_bytes())
        merged = {p: props for p, props in nodes.items() if p == TABLE or p.startswith(TABLE + '/')}
        if merged != table:
            raise ValueError('Merged tree does not exactly preserve saved GPU table')
        report['merged_exact_match'] = True
        report['merged_sha256'] = hashlib.sha256(args.verify_merged.read_bytes()).hexdigest()
    (ROOT / 'reports/gpu-dt-reconstruction.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Saved GPU table:', len(levels), 'levels; driver capacity:', limit, '; merged verified:', bool(args.verify_merged))


if __name__ == '__main__':
    main()
