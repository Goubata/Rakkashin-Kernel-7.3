#!/usr/bin/env python3
"""Disassemble the saved SOCCP provider for port planning; never load or execute it."""
import hashlib
import json
from pathlib import Path
import re
import shlex
import struct
import subprocess
from audit_module_versions import sections
from inventory_symbols import inspect

ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'

def main():
    path = ROOT / 'device-baseline/early-modules/0/lib/modules/qcom_q6v5_pas.ko'
    data = path.read_bytes()
    sec = sections(data)
    info = inspect(data)
    symbols = {}
    strings = sec['.strtab']
    for offset in range(0, len(sec['.symtab']), 24):
        name, kind, other, index, value, size = struct.unpack_from('<IBBHQQ', sec['.symtab'], offset)
        name = strings[name:].split(b'\0', 1)[0].decode() if name else ''
        if index and (kind & 15) == 2:
            symbols[name] = {'offset': value, 'bytes': size}
    out = ROOT / 'artifacts/reverse-analysis'
    out.mkdir(parents=True, exist_ok=True)
    selected = ['rproc_set_state']
    command = ['chroot', str(ROOTFS), '/usr/bin/llvm-objdump', '-dr',
               '--disassemble-symbols=' + ','.join(selected),
               '/work/avr-port/' + path.relative_to(ROOT).as_posix()]
    result = subprocess.run(['su', '-c', shlex.join(command)], capture_output=True, check=True)
    (out / 'qcom_q6v5_pas-rproc_set_state.disassembly.txt').write_bytes(result.stdout)
    text = result.stdout.decode()
    calls = sorted(set(re.findall(r'R_AARCH64_(?:CALL26|JUMP26)\s+(\S+)', text)))
    source = ROOT.parent / 'avr-upstream-audit/qcom-soc/drivers/remoteproc/qcom_q6v5_pas.c'
    tokens = [s.decode() for s in re.findall(rb'[\x20-\x7e]{5,}', data)]
    relevant = sorted({s for s in tokens if any(t in s.lower() for t in ['soccp', 'rproc-ddr', 'handover'])
                       and len(s) < 250})
    report = {'scope': 'static analysis of saved 6.12 binary and a public source candidate; not a working 7.3 implementation',
              'input': str(path.relative_to(ROOT)), 'input_sha256': hashlib.sha256(data).hexdigest(),
              'vermagic': info['vermagic'],
              'functions': {name: symbols.get(name) for name in selected},
              'exported_rproc_set_state': 'rproc_set_state' in info['exports'],
              'relocated_call_targets': calls,
              'relevant_binary_strings': relevant,
              'candidate_source_commit': '037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1',
              'candidate_source_sha256': hashlib.sha256(source.read_bytes()).hexdigest(),
              'analysis_output': 'artifacts/reverse-analysis/qcom_q6v5_pas-rproc_set_state.disassembly.txt',
              'analysis_sha256': hashlib.sha256(result.stdout).hexdigest(),
              'must_preserve': ['Reference-counted SOCCP power votes and SSR handling',
                                'V2 power payload, control queue, IPCC notification',
                                'Firmware acknowledgement and timeout/error handling'],
              'not_equivalent': 'Assigning rproc->state or returning success without a firmware transition',
              'command': command}
    report['observations'] = ['Saved PAS binary does not export rproc_set_state' if not report['exported_rproc_set_state'] else 'Saved PAS binary exports rproc_set_state']
    hwpath = ROOT / 'device-baseline/early-modules/0/lib/modules/msm_hw_fence.ko'
    hwdata = hwpath.read_bytes()
    hwinfo = inspect(hwdata)
    hwcommand = ['chroot', str(ROOTFS), '/usr/bin/llvm-objdump', '-dr',
                 '--disassemble-symbols=_set_soccp_fw_state_v1,_set_soccp_fw_state_v2',
                 '/work/avr-port/' + hwpath.relative_to(ROOT).as_posix()]
    hwresult = subprocess.run(['su', '-c', shlex.join(hwcommand)], capture_output=True, check=True)
    (out / 'msm_hw_fence-soccp-power.disassembly.txt').write_bytes(hwresult.stdout)
    hwtext = hwresult.stdout.decode()
    report['stock_hw_fence'] = {
        'sha256': hashlib.sha256(hwdata).hexdigest(),
        'imports_rproc_set_state': 'rproc_set_state' in hwinfo['imports'],
        'v2_present': '<_set_soccp_fw_state_v2>:' in hwtext,
        'relocated_call_targets': sorted(set(re.findall(r'R_AARCH64_(?:CALL26|JUMP26)\s+(\S+)', hwtext))),
        'disassembly_sha256': hashlib.sha256(hwresult.stdout).hexdigest(),
        'disassembly': 'artifacts/reverse-analysis/msm_hw_fence-soccp-power.disassembly.txt'}
    report['interpretation'] = 'The saved stock modules do not require the V1 rproc_set_state export. Review and preserve the real V2 queue/IPCC/ack path; do not invent an always-success V1 provider.'
    original = subprocess.check_output(['git', '-C', str(ROOT.parent / 'avr-upstream-audit/vendor-repos/vendor-qcom-opensource-mm-drivers'),
                                        'show', '6ac06dd09262b3bbe28c6d8105f18a42f8650832:hw_fence/src/hw_fence_drv_utils.c']).decode()
    ported = (ROOT / 'ports/qti-fence/hw_fence/src/hw_fence_drv_utils.c').read_text()
    def v2_region(text):
        start = text.index('static int _set_soccp_fw_state_v2(')
        return text[start:text.index('static int _init_soccp_props_ops(', start)]
    if v2_region(original) != v2_region(ported):
        raise ValueError('Review changes to real V2 power functions before recording preservation')
    report['ported_v2_power_functions_unchanged'] = True
    report['v2_source_region_sha256'] = hashlib.sha256(v2_region(ported).encode()).hexdigest()
    (ROOT / 'reports/soccp-binary-analysis.json').write_text(json.dumps(report, indent=2) + '\n')
    print(json.dumps({k: report[k] for k in ['functions', 'exported_rproc_set_state', 'relocated_call_targets']}, indent=2))

if __name__ == '__main__':
    main()
