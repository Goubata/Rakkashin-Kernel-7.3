#!/usr/bin/env python3
"""Recover observed IPCLite DT constants from pinned source and saved hardware DT."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import subprocess
from audit_boot_tree import fdt

ROOT = Path(__file__).resolve().parents[1]
AUDIT = ROOT.parent / 'avr-upstream-audit'
SOC_COMMIT = '037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
SYNX_COMMIT = '55b6a18a2a48ac12a258a80e85ea40daccd7ca62'
NAMES = ['IPCLITE_MSG_SIGNAL', 'IPCLITE_MEM_INIT_SIGNAL', 'IPCLITE_VERSION_SIGNAL',
         'IPCLITE_TEST_SIGNAL', 'IPCLITE_SSR_SIGNAL', 'IPCLITE_DEBUG_SIGNAL', 'MAX_CHANNEL_SIGNALS']


def source_blob(repo, commit, path):
    return subprocess.check_output(['git', '-C', str(repo), 'show', commit + ':' + path])


def cells(props, name, count):
    raw = props[name]
    if len(raw) != count * 4:
        raise ValueError('Unexpected cell count: ' + name)
    return [int.from_bytes(raw[i:i + 4], 'big') for i in range(0, len(raw), 4)]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--write', action='store_true')
    args = parser.parse_args()
    source = ROOT / 'ports/qti-fence/synx/msm/synx/ipclite.h'
    values = {name: int(re.search(r'^#define\s+' + name + r'\s+(\d+)\s*$', source.read_text(), re.M)[1])
              for name in NAMES}
    if [values[n] for n in NAMES] != list(range(7)):
        raise ValueError('Review changed driver signal-slot definitions')
    dtsi = source_blob(AUDIT / 'vendor-repos/kernel-synx-devicetree', SYNX_COMMIT, 'synx/canoe-synx.dtsi')
    headers = {path: source_blob(AUDIT / 'qcom-soc', SOC_COMMIT, path) for path in [
        'include/dt-bindings/mailbox/qcom-ipcc.h', 'include/dt-bindings/mailbox/qcom-ipcc-canoe.h']}
    clients = {}
    for raw in headers.values():
        for name, value in re.findall(r'^#define\s+(IPCC_\w+)\s+(0x[0-9a-fA-F]+|\d+)\s*$', raw.decode(), re.M):
            clients[name] = int(value, 0)
    saved = ROOT / 'artifacts/dt-input-audit/stock-merged.dtb'
    nodes, _ = fdt(saved.read_bytes())
    provider = nodes['/soc/qcom,ipcc_compute_l0@11c3000']
    parent = cells(provider, 'phandle', 1)[0]
    if provider['compatible'] != b'qcom,ipcc\0' or cells(provider, '#mbox-cells', 1) != [2] or cells(provider, '#interrupt-cells', 1) != [3]:
        raise ValueError('Review saved IPCC provider contract')
    observed = []
    hardware = {}
    for peer, body in re.findall(r'\bipclite_\w+:\s*(\w+)\s*\{(.*?)\n\t\t\};', dtsi.decode(), re.S):
        for slot, block in re.findall(r'\bipclite_signal_(\d+)\s*\{([^{}]*)\};', body):
            path = '/soc/ipclite/' + peer + '/ipclite_signal_' + slot
            props = nodes[path]
            index = int(slot)
            if cells(props, 'index', 1) != [index] or index >= values['MAX_CHANNEL_SIGNALS']:
                raise ValueError('Saved index does not match source: ' + path)
            mbox = re.search(r'mboxes\s*=\s*<&ipcc_compute_l0\s+(IPCC_\w+)\s+(IPCC_COMPUTE_L0_SIGNAL_IPCLITE_\w+)\s*>;', block)
            irq = re.search(r'interrupts\s*=\s*<(IPCC_\w+)\s+(IPCC_COMPUTE_L0_SIGNAL_IPCLITE_\w+)\s+IRQ_TYPE_EDGE_RISING\s*>;', block)
            if not mbox or not irq or mbox.groups() != irq.groups():
                raise ValueError('Review source mailbox/interrupt mapping: ' + path)
            client, symbol = mbox.groups()
            mb = cells(props, 'mboxes', 3)
            interrupts = cells(props, 'interrupts', 3)
            if mb[:2] != [parent, clients[client]] or interrupts != [clients[client], mb[2], 1] or cells(props, 'interrupt-parent', 1) != [parent]:
                raise ValueError('Saved mailbox/interrupt disagrees with source: ' + path)
            if symbol in hardware and hardware[symbol] != mb[2]:
                raise ValueError('Inconsistent hardware signal: ' + symbol)
            hardware[symbol] = mb[2]
            observed.append({'node': path, 'index': index, 'client_macro': client,
                             'client_id': mb[1], 'hardware_signal_macro': symbol, 'hardware_signal_id': mb[2]})
    expected = {p for p, props in nodes.items() if p.startswith('/soc/ipclite/') and 'index' in props}
    if {r['node'] for r in observed} != expected or len(observed) != 42 or len(hardware) != 6:
        raise ValueError('Source/saved signal coverage mismatch')
    content = '''/* SPDX-License-Identifier: GPL-2.0-only */
/* AVR compatibility definitions reconstructed from the IPCLite driver,
 * pinned canoe-synx.dtsi and 42 saved DT mailbox/interrupt entries.
 * Slot indices and hardware signal IDs were checked independently; their
 * equal values on this saved device are not an assumed general rule.
 * This is owned compatibility data, not the recovered original vendor header.
 */
#ifndef AVR_DT_IPCLITE_SIGNALS_H
#define AVR_DT_IPCLITE_SIGNALS_H
'''
    content += ''.join('#define ' + name + ' ' + str(values[name]) + '\n' for name in NAMES)
    content += '\n/* COMPUTE_L0 hardware signal IDs observed on AVR. */\n'
    content += ''.join('#define ' + name + ' ' + str(value) + '\n' for name, value in sorted(hardware.items())) + '#endif\n'
    target = ROOT / 'ports/device-tree/compat/dt-bindings/ipclite-signals.h'
    if args.write:
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(content)
    if target.read_text() != content:
        raise ValueError('Compatibility binding differs from verified inputs')
    report = {'scope': 'saved DT/source contract only; not firmware IPC operation or recovery of the absent vendor header',
              'passed': True, 'vendor_header_recovered': False, 'constants': values, 'hardware_signal_constants': hardware,
              'driver_header_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'synx_commit': SYNX_COMMIT, 'synx_dtsi_sha256': hashlib.sha256(dtsi).hexdigest(),
              'soc_commit': SOC_COMMIT, 'client_header_sha256': {p: hashlib.sha256(b).hexdigest() for p, b in headers.items()},
              'saved_dtb_sha256': hashlib.sha256(saved.read_bytes()).hexdigest(),
              'saved_signal_entries': observed, 'output': str(target.relative_to(ROOT)),
              'output_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (ROOT / 'reports/ipclite-dt-binding-reconstruction.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Verified six slots and six hardware signal IDs against', len(observed), 'saved DT entries')


if __name__ == '__main__':
    main()
