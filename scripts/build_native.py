#!/usr/bin/env python3
"""Build native GKI, optionally with initial Qualcomm firmware dependencies."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shlex
import subprocess
import sys

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
    parser.add_argument("--rakkashin", action="store_true", help="Port Rakkashin profiles, boosts, FAS, wake filters, legacy ntsync and ADIOS")
    parser.add_argument("--pmic-glink", action="store_true", help="Enable native QRTR/QMI/PDR dependencies; physical PMIC service acceptance separate")
    args = parser.parse_args()
    if args.pmic_glink:
        args.boot_core = True
    if args.rakkashin:
        args.boot_core = True
    if args.gpu_deps:
        args.boot_core = True
    if args.qemu_tests:
        args.boot_core = True
    if args.boot_core:
        args.platform = True
        args.pinctrl = True
        patches = [PORT/'ports/ccf-hold-state/hold-state-7.3.patch', PORT/'patches/irq-routing-7.3.patch', PORT/'patches/rcu-proxy-holdout-7.3.patch', PORT/'patches/shmem-folio-provider-7.3.patch', PORT/'patches/scm-gpu-dcvs-7.3.patch', PORT/'patches/scm-memory-batch-7.3.patch', PORT/'patches/scm-assign-notifier-7.3.patch', PORT/'patches/gunyah-core-api-7.3.patch', PORT/'patches/pas-shutdown-retry-7.3.patch', PORT/'patches/scm-ice-v2-7.3.patch', PORT/'patches/ufs-core-irq-stats-7.3.patch', PORT/'patches/diagnostic-symbol-api-7.3.patch', PORT/'patches/pdr-external-clients-7.3.patch', PORT/'patches/qmi-decode-bounds-7.3.patch', PORT/'patches/smem-state-lifetime-7.3.patch', PORT/'patches/geni-wrapper-accessor-7.3.patch', PORT/'patches/thermal-window-retry-7.3.patch', PORT/'patches/watchdog-native-api-7.3.patch', PORT/'patches/thermal-cooling-read-error-7.3.patch', PORT/'patches/thermal-binding-limits-7.3.patch', PORT/'patches/dynamic-task-vendor-data-7.3.patch', PORT/'patches/walt-core-hooks-7.3.patch', PORT/'patches/walt-core-exports-7.3.patch', PORT/'patches/moto-core-hooks-7.3.patch', PORT/'patches/dma-heap-lcp-7.3.patch', PORT/'patches/qtee-shmbridge-7.3.patch', PORT/'patches/scm-invoke-7.3.patch', PORT/'patches/tzmem-once-sleepable-7.3.patch']
        if args.rakkashin:
            patches.append(PORT/'patches/rakkashin-features-7.3.patch')
        checkpoint_path=PORT/'reports/core-patch-checkpoint.json'
        exact_checkpoint=False
        if checkpoint_path.is_file():
            checkpoint=json.loads(checkpoint_path.read_text())
            head=subprocess.check_output(['git','-C',str(SOURCE),'rev-parse','HEAD'],text=True).strip()
            clean=not subprocess.check_output(['git','-C',str(SOURCE),'status','--porcelain']).strip()
            exact_checkpoint=(clean and head==checkpoint['source_commit'] and
                all(checkpoint['patch_sha256'].get(p.relative_to(PORT).as_posix())==
                    hashlib.sha256(p.read_bytes()).hexdigest() for p in patches))
        for patch in patches:
            if exact_checkpoint:
                continue
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
    if args.rakkashin:
        root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                  'PATH=/usr/sbin:/usr/bin:/sbin:/bin', '/bin/bash',
                  '/work/avr-src/scripts/kconfig/merge_config.sh', '-m',
                  '-O', '/work/avr-port/out/native-gki',
                  '/work/avr-port/out/native-gki/.config',
                  '/work/avr-port/configs/rakkashin.fragment'])
        root_run(base + ['olddefconfig'])
    if args.qemu_tests:
        root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                  'PATH=/usr/sbin:/usr/bin:/sbin:/bin', '/bin/bash',
                  '/work/avr-src/scripts/kconfig/merge_config.sh', '-m',
                  '-O', '/work/avr-port/out/native-gki',
                  '/work/avr-port/out/native-gki/.config',
                  '/work/avr-port/configs/avr-qemu-tests.fragment'])
        root_run(base + ['olddefconfig'])
    if args.pmic_glink:
        root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                  'PATH=/usr/sbin:/usr/bin:/sbin:/bin', '/bin/bash',
                  '/work/avr-src/scripts/kconfig/merge_config.sh', '-m',
                  '-O', '/work/avr-port/out/native-gki',
                  '/work/avr-port/out/native-gki/.config',
                  '/work/avr-port/configs/avr-pmic-glink.fragment'])
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
    if args.rakkashin:
        root_run(base + ["M=/work/avr-port/ports/rakkashin/adios", "-j2", "modules"])
    if args.qemu_tests:
        if args.gpu_deps:
            root_run(base + ["M=/work/avr-port/tests/iommu", "-j2", "modules"])
        if args.rakkashin:
            root_run(base + ["headers"])
            root_run(base + ["M=/work/avr-port/tests/rakkashin-power", "-j2", "modules"])
            for name in ['rakkashin-test', 'adios-io-test']:
                root_run(['chroot', ROOTFS, '/usr/bin/env', '-i',
                          'PATH=/usr/sbin:/usr/bin:/sbin:/bin', 'cc', '-static', '-O2',
                          '-pthread', '-Wall', '-Wextra', '-Werror', '-fno-pie', '-no-pie',
                          '-I/work/avr-port/out/native-gki/usr/include',
                          '/work/avr-port/tests/rakkashin/'+name+'.c',
                          '-o', '/work/avr-port/tests/rakkashin/'+name])
            subprocess.run([sys.executable, str(PORT/'scripts/build_ntsync_selftest.py')], check=True)
        for suite in ["clock", "memory", "secure-buffer", "sysstats", "fence-compat"]:
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
