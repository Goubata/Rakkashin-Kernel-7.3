#!/usr/bin/env python3
"""Compile pinned component graphs and try an explicit research composition, never package."""
import argparse
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
    global BUNDLE, OUT
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--camera-reference',action='store_true')
    parser.add_argument('--extended-platform',action='store_true')
    parser.add_argument('--saved-gpu-table',action='store_true',help='Apply and exactly verify the user GPU table recovered from the saved device DT')
    args=parser.parse_args()
    if args.saved_gpu_table and not args.extended_platform:
        parser.error('--saved-gpu-table requires --extended-platform')
    lock_name='dt-camera-reference-lock.json' if args.camera_reference else 'dt-component-source-lock.json'
    report_name='dt-camera-reference-build.json' if args.camera_reference else 'dt-component-build.json'
    if args.camera_reference:
        BUNDLE=ROOT/'artifacts/dt-camera-reference-bundle'
        OUT=ROOT/'artifacts/dt-camera-reference-build'
    if args.extended_platform:
        BUNDLE=ROOT/'artifacts/dt-avr-platform-bundle'
        OUT=ROOT/'artifacts/dt-avr-platform-build'
        lock_name='dt-avr-platform-source-lock.json'
        report_name='dt-avr-platform-build.json'
    if args.saved_gpu_table:
        OUT=ROOT/'artifacts/dt-avr-platform-gpu-saved-build'
        report_name='dt-avr-platform-gpu-saved-build.json'
    lock = json.loads((ROOT / 'reports' / lock_name).read_text())
    if lock['status'] != 'include graph materialized':
        raise SystemExit('Component inputs incomplete; prepare and review them first')
    for item in lock['files']:
        path = BUNDLE / item['repository'] / item['path']
        if hashlib.sha256(path.read_bytes()).hexdigest() != item['sha256']:
            raise ValueError('Component source changed: ' + str(path))
    OUT.mkdir(parents=True, exist_ok=True)
    includes = [BUNDLE / repo / directory for repo in lock['repositories'] for directory in ['include', '.']]
    if 'avr-dt-compat' in lock['repositories']:
        includes.append(BUNDLE / 'avr-dt-compat/ports/device-tree/compat')
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
                              'alor-gpu','alor-hw-fence','alor-hfi-core','alor-vidc',
                              'alor-ipa','alor-smem-mailbox','canoe-synx',
                              'alor-avr-evb-overlay', 'alor-audio-moto-avr-evb',
                              'alor-sde-display-avr-evb-overlay', 'alor-camera-sensor-avr-evb',
                              'alor-wcn7750-bt-avr-evb','alor-qrd-wcn7750-avr-evb','alor-nfc-avr-evb']
             if name in outputs]
    saved_gpu_source = None
    if args.saved_gpu_table:
        subprocess.run(['python', str(ROOT/'scripts/reconstruct_gpu_dt.py')], check=True)
        saved_gpu_source=ROOT/'ports/device-tree/recovered/avr-saved-gpu-pwrlevels.dtso'
        output=OUT/'avr-saved-gpu-pwrlevels.dtb'
        commands['saved_gpu_dtc']=run([DTC+'dtc','-@','-H','epapr','-I','dts','-O','dtb',
                                      '-o',inside(output),inside(saved_gpu_source)],OUT/'saved-gpu-dtc.log')
        outputs['avr-saved-gpu-pwrlevels']=output
        order.append('avr-saved-gpu-pwrlevels')
    merged = OUT / 'avr-experimental-merged.dtb'
    command = [DTC + 'fdtoverlay', '-i', inside(outputs['alor-avr-base']), '-o', inside(merged)]
    command += [inside(outputs[name]) for name in order]
    commands['experimental_apply'] = run(command, OUT / 'experimental-apply.log')
    if args.saved_gpu_table:
        subprocess.run(['python', str(ROOT/'scripts/reconstruct_gpu_dt.py'), '--verify-merged', str(merged)], check=True)
    spec = importlib.util.spec_from_file_location('dt_audit', ROOT / 'scripts/audit_boot_tree.py')
    audit = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(audit)
    stock, _ = audit.fdt((ROOT / 'artifacts/dt-input-audit/stock-merged.dtb').read_bytes())
    rebuilt, _ = audit.fdt(merged.read_bytes())
    difference = audit.compare(stock, rebuilt)
    report = {'scope': 'component compilation and experimental ordering only; not an OEM recipe, binding validation or phone test',
              'source_file_count': len(lock['files']), 'source_lock': lock_name, 'source_lock_sha256': hashlib.sha256((ROOT/'reports'/lock_name).read_bytes()).hexdigest(), 'camera_reference_used': lock.get('camera_reference_used',False), 'extended_platform': args.extended_platform, 'camera_driver_compatibility_verified': False,
              'saved_gpu_table_used': args.saved_gpu_table,
              'saved_gpu_source_sha256': hashlib.sha256(saved_gpu_source.read_bytes()).hexdigest() if saved_gpu_source else None,
              'saved_gpu_table_exact_match': True if args.saved_gpu_table else None,
              'composition_order': order, 'oem_order_verified': False, 'packaging_allowed': False,
              'stock_node_count': len(stock), 'rebuilt_node_count': len(rebuilt),
              'missing_node_count': len(difference['left_only_nodes']),
              'extra_node_count': len(difference['right_only_nodes']),
              'difference': difference,
              'limited_reference_normalization': audit.semantic_compare(stock, rebuilt),
              'sha256': {name: hashlib.sha256(path.read_bytes()).hexdigest() for name, path in outputs.items()},
              'merged_sha256': hashlib.sha256(merged.read_bytes()).hexdigest(), 'commands': commands}
    (ROOT / 'reports' / report_name).write_text(json.dumps(audit.public_report(report), indent=2) + '\n')
    print(json.dumps({k: v for k, v in report.items() if k in
                      ('source_file_count', 'composition_order', 'stock_node_count', 'rebuilt_node_count',
                       'missing_node_count', 'extra_node_count', 'packaging_allowed')}, indent=2))

if __name__ == '__main__':
    main()
