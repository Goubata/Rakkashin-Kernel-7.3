#!/usr/bin/env python3
"""Compare saved-module evidence and real external-build flags for the AVF ABI."""
import hashlib
import json
from pathlib import Path
import re
import shlex
import subprocess
from inventory_symbols import inspect

ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def main():
    config = ROOT.parent / 'avr-upstream-audit/qcom-soc/configs/canoe_perf.bzl'
    if not re.search(r'"CONFIG_QTVM_WITH_AVF"\s*:\s*"y"', config.read_text()):
        raise ValueError('Review vendor AVF configuration')
    commands = ['ports/gunyah/.gh_dbl.o.cmd', 'ports/gunyah/.gh_msgq.o.cmd',
                'ports/gunyah-rm/.gh_rm_core.o.cmd',
                'ports/qti-fence/hw_fence/src/.hw_fence_drv_utils.o.cmd',
                'tests/remoteproc/.avr-gunyah-test.o.cmd']
    for relative in commands:
        if '-DCONFIG_QTVM_WITH_AVF=1' not in (ROOT / relative).read_text():
            raise ValueError('AVF shared-label flag missing from actual compile command: ' + relative)
    out = ROOT / 'artifacts/reverse-analysis'
    out.mkdir(parents=True, exist_ok=True)
    rows = {}
    for label, base in [('stock', ROOT / 'device-baseline/early-modules/0/lib/modules'),
                        ('port', ROOT / 'ports/gunyah')]:
        path = base / 'gh_dbl.ko'
        command = ['chroot', str(ROOTFS), '/usr/bin/llvm-objdump', '-d',
                   '--disassemble-symbols=gh_dbl_rx_register',
                   '/work/avr-port/' + path.relative_to(ROOT).as_posix()]
        result = subprocess.run(['su', '-c', shlex.join(command)], capture_output=True, check=True)
        (out / (label + '-gh-dbl-register.txt')).write_bytes(result.stdout)
        text = result.stdout.decode()
        # These are the range-validation instructions in both pinned binaries,
        # not proof that their whole struct layout or whole functions match.
        checks = [bool(re.search(r'sub\s+w\d+,\s*w0,\s*#0x10a', text)),
                  bool(re.search(r'cmn\s+w\d+,\s*#0xb', text))]
        rows[label] = {'sha256': sha(path), 'avf_range_checks_present': all(checks),
                       'disassembly_sha256': hashlib.sha256(result.stdout).hexdigest()}
        if not all(checks):
            raise ValueError('Review changed doorbell validation code: ' + label)
    notifier = 'gunyah_qtvm_register_notifier'
    paths = {'stock': ROOT / 'device-baseline/early-modules/0/lib/modules/gh_rm_drv.ko',
             'port': ROOT / 'ports/gunyah-rm/gh_rm_drv.ko'}
    imports = {name: notifier in inspect(path.read_bytes())['imports'] for name, path in paths.items()}
    if not all(imports.values()):
        raise ValueError('Real QTVM notifier import absent; check for optional-header success fallback')
    report = {'scope': 'selected AVF ABI evidence and build configuration only; not full binary equivalence or hypervisor runtime validation',
              'passed': True, 'vendor_config_commit': '037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1',
              'vendor_config_sha256': sha(config), 'flag': 'CONFIG_QTVM_WITH_AVF=y',
              'compile_commands_checked': commands, 'doorbell_range_evidence': rows,
              'real_qtvm_notifier_import': imports,
              'required_hw_fence_doorbell_label': '0x106', 'message_queue_min_label': '0x101',
              'structure_note': 'Kernel-private spinlock/lockdep layout may differ; import CRCs and wire fields are checked separately'}
    (ROOT / 'reports/gunyah-avf-abi-audit.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Saved/ported AVF doorbell range, QTVM import and external compile flags agree')

if __name__ == '__main__':
    main()
