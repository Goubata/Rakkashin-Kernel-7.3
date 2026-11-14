#!/usr/bin/env python3
"""Build staged SSR/Gunyah modules against an existing configured 7.3 tree."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tests', action='store_true')
    parser.add_argument('--rm-research',action='store_true',help='Attempt unfinished RM link; currently fails on real transport/SCM providers')
    args = parser.parse_args()
    for relative in ['out/native-gki/Module.symvers', 'ports/qcom-smem/Module.symvers',
                     'ports/qcom-hwspinlock/Module.symvers']:
        if not (ROOT / relative).is_file():
            raise SystemExit('Build core/GPU dependencies first: missing ' + relative)
    subprocess.run([sys.executable, str(ROOT / 'scripts/audit_ipc_sources.py')], check=True)
    targets = ['ports/qcom-rproc-common', 'ports/gunyah']
    if args.rm_research:
        targets.append('ports/gunyah-rm')
    if args.tests:
        targets.append('tests/remoteproc')
    for target in targets:
        command = ['chroot', str(ROOTFS), '/usr/bin/env', '-i', 'PATH=/usr/sbin:/usr/bin:/sbin:/bin',
                   'make', '-C', '/work/avr-src', 'O=/work/avr-port/out/native-gki',
                   'M=/work/avr-port/' + target, 'ARCH=arm64', 'LLVM=1', '-j2', 'modules']
        subprocess.run(['su', '-c', shlex.join(command)], check=True)

if __name__ == '__main__':
    main()
