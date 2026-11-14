#!/usr/bin/env python3
"""Build pinned AVR storage, diagnostic, regulator and interconnect ports; no device writes."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--ice',action='store_true')
    parser.add_argument('--tests',action='store_true')
    parser.add_argument('--ufs-host',action='store_true',help='Build real diagnostic dependencies and vendor host; no physical probe')
    parser.add_argument('--ufs-core-tests',action='store_true')
    parser.add_argument('--diagnostics',action='store_true')
    parser.add_argument('--diagnostics-tests',action='store_true')
    parser.add_argument('--regulators',action='store_true')
    parser.add_argument('--regulators-tests',action='store_true')
    parser.add_argument('--interconnect',action='store_true')
    parser.add_argument('--interconnect-tests',action='store_true')
    args=parser.parse_args()
    if args.ufs_host:args.ice=True
    if args.interconnect_tests:args.interconnect=True
    if args.regulators_tests:args.regulators=True
    if args.ufs_host or args.diagnostics_tests or args.regulators:args.diagnostics=True
    if args.tests and not args.ice:parser.error('--tests requires --ice')
    report = json.loads((ROOT / 'reports/ufs-phy-source-lock.json').read_text())
    repo = ROOT.parent / 'avr-upstream-audit/qcom-soc'
    for row in report['files']:
        raw = subprocess.check_output(['git', '-C', str(repo), 'show', report['source_commit'] + ':' + row['source_path']])
        if hashlib.sha256(raw).hexdigest() != row['original_sha256'] or hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest() != row['original_git_blob']:
            raise ValueError('Pinned source lock mismatch: ' + row['source_path'])
        if (ROOT / row['staged_path']).read_bytes() != raw:
            raise ValueError('Staged PHY source differs; review and record port changes explicitly: ' + row['staged_path'])
    print('Verified five unmodified firmware-pinned UFS PHY source files', flush=True)
    targets=['ports/ufs-phy-canoe']
    if args.ice:
        subprocess.run([sys.executable,str(ROOT/'scripts/audit_ice_sources.py')],check=True)
        config=(ROOT/'out/native-gki/.config').read_text()
        if 'CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE=y' not in config:
            raise ValueError('AVR ICE requires the firmware-matched SHM Bridge configuration')
        symbols={line.split()[1] for line in (ROOT/'out/native-gki/Module.symvers').read_text().splitlines() if line.strip()}
        needed={'qcom_scm_ice_v2_available','qcom_scm_ice_set_key_v2',
                'qcom_scm_ice_clear_key_v2','qcom_scm_ice_derive_secret_v2'}
        if not needed <= symbols:raise ValueError('Build the ICE V2 core patch first')
        if 'qcom_ice_enable' in symbols:raise ValueError('Native ICE and vendor ICE must not provide the same symbols')
        targets.append('ports/qcom-ice')
        if args.tests:targets.append('tests/ice')
    if args.diagnostics:
        subprocess.run([sys.executable,str(ROOT/'scripts/audit_diagnostics_sources.py')],check=True)
        targets += ['ports/qcom-minidump','ports/qcom-ipc-logging']
        if args.diagnostics_tests:targets += ['tests/debug-symbol','tests/diagnostics']
    if args.ufs_host:
        subprocess.run([sys.executable,str(ROOT/'scripts/audit_ufs_sources.py')],check=True)
        targets.append('ports/ufs-host')
    if args.regulators:
        subprocess.run([sys.executable,str(ROOT/'scripts/audit_regulators_sources.py')],check=True)
        targets.append('ports/qcom-regulators')
        if args.regulators_tests:targets.append('tests/regulators')
    if args.interconnect:
        subprocess.run([sys.executable,str(ROOT/'scripts/audit_interconnect_sources.py')],check=True)
        targets.append('ports/qcom-interconnect')
        if args.interconnect_tests:targets.append('tests/interconnect')
    if args.ufs_core_tests:targets.append('tests/ufs')
    for target in targets:
        command = ['chroot', str(ROOTFS), '/usr/bin/env', '-i', 'PATH=/usr/sbin:/usr/bin:/sbin:/bin',
                   'KBUILD_BUILD_USER=avr-builder', 'KBUILD_BUILD_HOST=avr-arm64',
                   'make', '-C', '/work/avr-src', 'O=/work/avr-port/out/native-gki',
                   'M=/work/avr-port/'+target, 'ARCH=arm64', 'LLVM=1', '-j2', 'modules']
        subprocess.run(['su', '-c', shlex.join(command)], check=True)
    if args.ufs_core_tests:
        command=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin',
                 'cc','-static','-O2','-Wall','-Wextra','-Werror',
                 '/work/avr-port/tests/ufs/ufs-io-test.c','-o','/work/avr-port/tests/ufs/ufs-io-test']
        subprocess.run(['su','-c',shlex.join(command)],check=True)


if __name__ == '__main__':
    main()
