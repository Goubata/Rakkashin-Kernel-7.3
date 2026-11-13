#!/usr/bin/env python3
"""Build native GKI, optionally with initial Qualcomm firmware dependencies."""
from pathlib import Path
import argparse
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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--platform", action="store_true", help="Enable initial SCM/PAS/TZMEM dependencies; not complete avr support")
    parser.add_argument("--pinctrl", action="store_true", help="Also link the staged Alor pinctrl modules; requires --platform")
    parser.add_argument("--boot-core", action="store_true", help="Apply core patches and build clock/RPMh/IRQ/pinctrl and OxygenOS sync modules")
    parser.add_argument("--qemu-tests", action="store_true", help="Build boot-core plus isolated clock/RCU and fence test fixtures")
    parser.add_argument("--gpu-deps", action="store_true", help="Build ported GPU support modules; KGSL and physical SMMU bring-up remain incomplete")
    args = parser.parse_args()
    if args.gpu_deps:
        args.boot_core = True
    if args.qemu_tests:
        args.boot_core = True
    if args.boot_core:
        args.platform = True
        args.pinctrl = True
        for patch in [PORT/'ports/ccf-hold-state/hold-state-7.3.patch', PORT/'patches/irq-routing-7.3.patch', PORT/'patches/rcu-proxy-holdout-7.3.patch', PORT/'patches/shmem-folio-provider-7.3.patch', PORT/'patches/scm-gpu-dcvs-7.3.patch', PORT/'patches/scm-memory-batch-7.3.patch']:
            command=['git','-C',str(SOURCE),'apply']
            if subprocess.run(command+['--reverse','--check',str(patch)],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL).returncode == 0:
                continue
            subprocess.run(command+['--check',str(patch)],check=True)
            subprocess.run(command+[str(patch)],check=True)

    if args.pinctrl and not args.platform:
        parser.error("--pinctrl requires --platform")
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
    if args.platform:
        root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                  'PATH=/usr/sbin:/usr/bin:/sbin:/bin', '/bin/bash',
                  '/work/avr-src/scripts/kconfig/merge_config.sh', '-m',
                  '-O', '/work/avr-port/out/native-gki',
                  '/work/avr-port/out/native-gki/.config',
                  '/work/avr-port/configs/avr-platform.fragment'])
        root_run(base + ['olddefconfig'])
    if args.qemu_tests:
        root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                  'PATH=/usr/sbin:/usr/bin:/sbin:/bin', '/bin/bash',
                  '/work/avr-src/scripts/kconfig/merge_config.sh', '-m',
                  '-O', '/work/avr-port/out/native-gki',
                  '/work/avr-port/out/native-gki/.config',
                  '/work/avr-port/configs/avr-qemu-tests.fragment'])
        root_run(base + ['olddefconfig'])
    root_run(base + ['-j' + os.environ.get('BUILD_JOBS', '4'), 'Image', 'modules'])
    if args.boot_core:
        for name in ['qcom-clock-core', 'qcom-rpmh', 'qcom-clock-controllers', 'qcom-irq', 'avr-oplus-sync']:
            root_run(base + ["M=/work/avr-port/ports/"+name, "-j"+os.environ.get('BUILD_JOBS','4'), "modules"])
    if args.pinctrl:
        root_run(base + ["M=/work/avr-port/ports/pinctrl-alor-vendor-core", "-j2", "modules"])

    if args.gpu_deps:
        for name in ['qcom-hwspinlock', 'qcom-smem', 'qcom-socinfo', 'qcom-llcc',
                     'qcom-dcvs', 'governor-adreno-ro', 'governor-gpubw',
                     'governor-adreno-tz', 'qcom-iommu-util', 'secure-buffer', 'iommu-logger', 'arm-smmu-vendor','msm-sysstats']:
            root_run(base + ["M=/work/avr-port/ports/"+name, "-j"+os.environ.get('BUILD_JOBS','4'), "modules"])
    if args.qemu_tests:
        if args.gpu_deps:
            root_run(base + ["M=/work/avr-port/tests/iommu", "-j2", "modules"])
        for suite in ["clock", "memory", "secure-buffer", "sysstats"]:
            root_run(base + ["M=/work/avr-port/tests/"+suite, "-j2", "modules"])
        root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                  'PATH=/usr/sbin:/usr/bin:/sbin:/bin', 'cc', '-static', '-O2',
                  '-Wall', '-Wextra', '-Werror',
                  '/work/avr-port/tests/sysstats/sysstats-netlink-test.c',
                  '-o', '/work/avr-port/tests/sysstats/sysstats-netlink-test'])
        root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                  'PATH=/usr/sbin:/usr/bin:/sbin:/bin', 'cc', '-static', '-O2',
                  '-Wall', '-Wextra', '-Werror',
                  '/work/avr-port/tests/oplus-sync/oplus-sync-test.c',
                  '-o', '/work/avr-port/tests/oplus-sync/oplus-sync-test'])

if __name__ == '__main__':
    main()
