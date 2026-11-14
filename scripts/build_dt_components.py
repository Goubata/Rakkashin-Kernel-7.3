#!/usr/bin/env python3
"""Compile pinned component graphs and try an explicit research composition, never package."""
import hashlib
import importlib.util
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
BUNDLE = ROOT / 'artifacts/dt-component-source-bundle'
OUT = ROOT / 'artifacts/dt-component-build'
ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'
DTC = '/work/avr-port/out/native-gki/scripts/dtc/'

def inside(path):
    return '/work/avr-port/' + path.relative_to(ROOT).as_posix()

def run(command, log):
    full = ['chroot', str(ROOTFS), '/usr/bin/env', '-i', 'PATH=/usr/sbin:/usr/bin:/sbin:/bin'] + command
    with log.open('wb') as stream:
        subprocess.run(['su', '-c', shlex.join(full)], stdout=stream, stderr=subprocess.STDOUT, check=True)
    return full

def main():
    lock = json.loads((ROOT / 'reports/dt-component-source-lock.json').read_text())
    if lock['status'] != 'include graph materialized':
        raise SystemExit('Component inputs incomplete; prepare and review them first')
    for item in lock['files']:
        path = BUNDLE / item['repository'] / item['path']
        if hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Component source changed: ' + str(path))
    OUT.mkdir(parents=True, exist_ok=True)
    includes = [BUNDLE / repo / 'include' for repo in lock['repositories']]
    includes += sorted({(BUNDLE / row['repository'] / row['path']).parent for row in lock['files']})
    commands = {}
    outputs = {}
    for key in lock['roots']:
        repo, relative = key.split(':', 1)
        name = Path(relative).stem
        prepared = OUT / (name + '.preprocessed.dts')
        output = OUT / (name + '.dtb')
        cpp = ['cc', '-E', '-P', '-nostdinc', '-undef', '-D__DTS__', '-x', 'assembler-with-cpp']
        for directory in includes:
            cpp += ['-I', inside(directory)]
        cpp += [inside(BUNDLE / repo / relative), '-o', inside(prepared)]
        commands[name + '_cpp'] = run(cpp, OUT / (name + '-cpp.log'))
        commands[name + '_dtc'] = run([DTC + 'dtc', '-@', '-H', 'epapr', '-I', 'dts', '-O', 'dtb',
                                      '-o', inside(output), inside(prepared)], OUT / (name + '-dtc.log'))
        outputs[name] = output
    # This is a hypothesis for analysis, NOT the reconstructed OEM build recipe.
    # Keep component base extensions before board-specific overrides.
    order = [name for name in ['alor-audio', 'alor-sde', 'alor-camera',
                              'alor-avr-evb-overlay', 'alor-audio-moto-avr-evb',
                              'alor-sde-display-avr-evb-overlay', 'alor-camera-sensor-avr-evb']
             if name in outputs]
    merged = OUT / 'avr-experimental-merged.dtb'
    command = [DTC + 'fdtoverlay', '-i', inside(outputs['alor-avr-base']), '-o', inside(merged)]
    command += [inside(outputs[name]) for name in order]
    commands['experimental_apply'] = run(command, OUT / 'experimental-apply.log')
    spec = importlib.util.spec_from_file_location('dt_audit', ROOT / 'scripts/audit_boot_tree.py')
    audit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(audit)
    stock, _ = audit.fdt((ROOT / 'artifacts/dt-input-audit/stock-merged.dtb').read_bytes())
    rebuilt, _ = audit.fdt(merged.read_bytes())
    difference = audit.compare(stock, rebuilt)
    report = {'scope': 'component compilation and experimental ordering only; not an OEM recipe, binding validation or phone test',
              'source_file_count': len(lock['files']), 'source_lock': 'dt-component-source-lock.json',
              'composition_order': order, 'oem_order_verified': False, 'packaging_allowed': False,
              'stock_node_count': len(stock), 'rebuilt_node_count': len(rebuilt),
              'missing_node_count': len(difference['left_only_nodes']),
              'extra_node_count': len(difference['right_only_nodes']),
              'difference': difference,
              'limited_reference_normalization': audit.semantic_compare(stock, rebuilt),
              'sha256': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in outputs.items()},
              'merged_sha256': hashlib.sha256(merged.read_bytes()).hexdigest(), 'commands': commands}
    (ROOT / 'reports/dt-component-build.json').write_text(json.dumps(audit.public_report(report), indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k in
                      ('source_file_count', 'composition_order', 'stock_node_count', 'rebuilt_node_count',
                       'missing_node_count', 'extra_node_count', 'packaging_allowed')}, indent=2))

if __name__ == '__main__':
    main()
