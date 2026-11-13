#!/usr/bin/env python3
"""Build the baseline in the existing on-device Ubuntu arm64 chroot."""
from pathlib import Path
import os
import shlex
import subprocess

PORT = Path(__file__).resolve().parents[1]
HOME_DIR = PORT.parent
ROOTFS = HOME_DIR / '.andronix/distros/ubuntu/rootfs'
SOURCE = HOME_DIR / 'avr-upstream-audit/android-7.3'

def root_run(args):
    subprocess.run(['su', '-c', shlex.join([str(a) for a in args])], check=True)

def main():
    if not (SOURCE / 'Makefile').is_file() or not (ROOTFS / 'usr/bin/make').is_file():
        raise SystemExit('Source checkout and Ubuntu build tools are required.')
    for origin, relative in [('/proc', 'proc'), ('/dev', 'dev'),
                             (str(SOURCE), 'work/avr-src'), (str(PORT), 'work/avr-port')]:
        target = ROOTFS / relative
        target.mkdir(parents=True, exist_ok=True)
        probe = subprocess.run(['su', '-c', shlex.join(['mountpoint', '-q', str(target)])])
        if probe.returncode:
            root_run(['mount', '--bind', origin, target])
    base = ['chroot', ROOTFS, '/usr/bin/env', '-i',
            'PATH=/usr/sbin:/usr/bin:/sbin:/bin', 'KBUILD_BUILD_USER=avr-builder',
            'KBUILD_BUILD_HOST=avr-arm64', '/usr/bin/make', '-C', '/work/avr-src',
            'O=/work/avr-port/out/native-gki', 'ARCH=arm64', 'LLVM=1']
    root_run(base + ['rustavailable'])
    root_run(base + ['gki_defconfig'])
    root_run(base + ['-j' + os.environ.get('BUILD_JOBS', '4'), 'Image', 'modules'])

if __name__ == '__main__':
    main()
