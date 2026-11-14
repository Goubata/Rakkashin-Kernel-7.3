#!/usr/bin/env python3
"""Verify pinned Canoe UFS PHY sources and build against configured 7.3; no device access."""
import hashlib
import json
from pathlib import Path
import shlex
import subprocess

ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'


def main():
    report = json.loads((ROOT / 'reports/ufs-phy-source-lock.json').read_text())
    repo = ROOT.parent / 'avr-upstream-audit/qcom-soc'
    for row in report['files']:
        raw = subprocess.check_output(['git', '-C', str(repo), 'show', report['source_commit'] + ':' + row['source_path']])
        if hashlib.sha256(raw).hexdigest() != row['original_sha256'] or hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest() != row['original_git_blob']:
            raise ValueError('Pinned source lock mismatch: ' + row['source_path'])
        if (ROOT / row['staged_path']).read_bytes() != raw:
            raise ValueError('Staged PHY source differs; review and record port changes explicitly: ' + row['staged_path'])
    print('Verified five unmodified firmware-pinned UFS PHY source files', flush=True)
    command = ['chroot', str(ROOTFS), '/usr/bin/env', '-i', 'PATH=/usr/sbin:/usr/bin:/sbin:/bin',
               'KBUILD_BUILD_USER=avr-builder', 'KBUILD_BUILD_HOST=avr-arm64',
               'make', '-C', '/work/avr-src', 'O=/work/avr-port/out/native-gki',
               'M=/work/avr-port/ports/ufs-phy-canoe', 'ARCH=arm64', 'LLVM=1', '-j2', 'modules']
    subprocess.run(['su', '-c', shlex.join(command)], check=True)


if __name__ == '__main__':
    main()
