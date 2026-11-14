#!/usr/bin/env python3
"""Recover three stable saved AVR memory definitions, never bootloader state."""
import argparse
import hashlib
import json
from pathlib import Path
from audit_boot_tree import fdt

ROOT = Path(__file__).resolve().parents[1]
POOL = '/reserved-memory/non_secure_display_region'
FIXED = '/reserved-memory/reserved_region@80e00000'
HEAP = '/soc/qcom,dma-heaps/qcom,display'
TARGET = ROOT / 'ports/device-tree/recovered/avr-saved-memory.dtso'
EXPECTED = {
    POOL: {'compatible', 'reusable', 'alloc-ranges', 'size', 'alignment', 'phandle'},
    FIXED: {'no-map', 'reg', 'phandle'},
    HEAP: {'qcom,dma-heap-name', 'qcom,dma-heap-type', 'qcom,max-align', 'memory-region'},
}


def mapped_property(nodes, name, value):
    if name != 'memory-region':
        return value
    if len(value) != 4:
        raise ValueError('Expected one memory-region reference')
    matches = [p for p, props in nodes.items() if props.get('phandle') == value]
    if matches != [POOL]:
        raise ValueError('Display heap must refer to the recovered pool')
    return POOL


def comparable(nodes, path):
    props = nodes[path]
    if set(props) != EXPECTED[path]:
        raise ValueError('Review unexpected properties: ' + path)
    return {k: mapped_property(nodes, k, v) for k, v in props.items() if k != 'phandle'}


def literal(name, value):
    if not value:
        return name + ';'
    if name in ('compatible', 'qcom,dma-heap-name'):
        text = value.rstrip(b'\0').decode('ascii')
        if any(c not in 'abcdefghijklmnopqrstuvwxyz,-' for c in text):
            raise ValueError('Unexpected string in memory definitions')
        return name + ' = "' + text + '";'
    if len(value) % 4:
        raise ValueError('Expected aligned cells')
    return name + ' = <' + ' '.join('0x' + value[i:i + 4].hex() for i in range(0, len(value), 4)) + '>;'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    parser.add_argument('--verify-merged', type=Path)
    args = parser.parse_args()
    saved = ROOT / 'artifacts/dt-input-audit/stock-merged.dtb'
    running = ROOT / 'device-baseline/running.dtb'
    nodes, _ = fdt(saved.read_bytes())
    live, _ = fdt(running.read_bytes())
    for path in EXPECTED:
        if comparable(nodes, path) != comparable(live, path):
            raise ValueError('Static/running memory definition changed: ' + path)
    pool = nodes[POOL]
    if pool['compatible'] != b'shared-dma-pool\0' or pool['reusable'] != b'':
        raise ValueError('Expected reusable display CMA pool')
    if len(pool['size']) != 8 or len(pool['alignment']) != 8 or len(pool['alloc-ranges']) != 16:
        raise ValueError('Expected 64-bit saved memory encoding')
    size = int.from_bytes(pool['size'], 'big')
    alignment = int.from_bytes(pool['alignment'], 'big')
    if not size or not alignment or alignment & (alignment - 1) or size % alignment:
        raise ValueError('Invalid saved display pool alignment')
    fixed = nodes[FIXED]
    if len(fixed['reg']) != 16 or fixed['no-map'] != b'':
        raise ValueError('Expected one 64-bit fixed no-map interval')
    address = int.from_bytes(fixed['reg'][:8], 'big')
    length = int.from_bytes(fixed['reg'][8:], 'big')
    if not length or address + length > 1 << 64:
        raise ValueError('Invalid fixed interval')
    # The restored fixed interval must not intersect another statically reserved
    # interval in the saved DT. Dynamic size/alloc-ranges pools stay dynamic.
    for path, props in nodes.items():
        if not path.startswith('/reserved-memory/') or path == FIXED or 'reg' not in props:
            continue
        raw = props['reg']
        if len(raw) % 16:
            raise ValueError('Review non-64-bit reserved memory interval: ' + path)
        for off in range(0, len(raw), 16):
            start = int.from_bytes(raw[off:off + 8], 'big')
            count = int.from_bytes(raw[off + 8:off + 16], 'big')
            if count and address < start + count and start < address + length:
                raise ValueError('Fixed reservation overlaps saved region: ' + path)
    lines = ['/* SPDX-License-Identifier: GPL-2.0-only */',
             '/* Stable definitions recovered from saved static AND running AVR DT.',
             ' * Dynamic pool allocation remains dynamic; no bootloader RAM or',
             ' * hypervisor runtime addresses are copied into this source. */',
             '/dts-v1/;', '/plugin/;', '', '/ {', '\tfragment@0 {',
             '\t\ttarget-path = "/reserved-memory";', '\t\t__overlay__ {',
             '\t\t\tavr_saved_display_pool: non_secure_display_region {']
    lines += ['\t\t\t\t' + literal(k, v) for k, v in sorted(pool.items()) if k != 'phandle']
    lines += ['\t\t\t};', '\t\t};', '\t};', '\tfragment@1 {',
              '\t\ttarget-path = "' + FIXED + '";', '\t\t__overlay__ {']
    lines += ['\t\t\t' + literal(k, v) for k, v in sorted(fixed.items()) if k != 'phandle']
    lines += ['\t\t};', '\t};', '\tfragment@2 {', '\t\ttarget-path = "/soc/qcom,dma-heaps";',
              '\t\t__overlay__ {', '\t\t\tqcom,display {']
    lines += ['\t\t\t\t' + literal(k, v) for k, v in sorted(nodes[HEAP].items()) if k != 'memory-region']
    lines += ['\t\t\t\tmemory-region = <&avr_saved_display_pool>;', '\t\t\t};', '\t\t};', '\t};', '};', '']
    content = '\n'.join(lines)
    if args.write:
        TARGET.parent.mkdir(parents=True, exist_ok=True)
        TARGET.write_text(content)
    if TARGET.read_text() != content:
        raise ValueError('Recovered memory source differs from saved inputs')
    report = {'scope': 'saved stable memory definitions and resolved display heap reference only; not CMA allocation, DMA heap driver, firmware or device runtime proof',
              'static_and_running_definitions_match': True,
              'input_sha256': {str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest() for p in (saved, running)},
              'output': str(TARGET.relative_to(ROOT)), 'output_sha256': hashlib.sha256(TARGET.read_bytes()).hexdigest(),
              'nodes': list(EXPECTED), 'display_pool_size': size, 'display_pool_alignment': alignment,
              'fixed_region_address': address, 'fixed_region_size': length,
              'fixed_region_saved_overlap_check_passed': True, 'packaging_allowed': False}
    if args.verify_merged:
        merged, _ = fdt(args.verify_merged.read_bytes())
        for path in EXPECTED:
            if comparable(merged, path) != comparable(nodes, path):
                raise ValueError('Merged memory definitions differ: ' + path)
        report['merged_values_and_reference_match'] = True
        report['merged_sha256'] = hashlib.sha256(args.verify_merged.read_bytes()).hexdigest()
    (ROOT / 'reports/memory-dt-reconstruction.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Verified/recovered three memory nodes; saved display pool:', size, 'bytes; merged verified:', bool(args.verify_merged))


if __name__ == '__main__':
    main()
