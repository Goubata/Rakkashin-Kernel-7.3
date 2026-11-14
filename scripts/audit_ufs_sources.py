#!/usr/bin/env python3
"""Lock/replay the in-progress vendor UFS host port without claiming link success."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent / 'avr-upstream-audit/qcom-soc'
COMMIT = '037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
FILES = {
    'ufs-qcom-main.c': 'drivers/ufs/host/ufs-qcom.c',
    'ufs-qcom.h': 'drivers/ufs/host/ufs-qcom.h',
    'ufs-qcom-trace.h': 'drivers/ufs/host/ufs-qcom-trace.h',
    'ufshcd-crypto-qti.c': 'drivers/ufs/host/ufshcd-crypto-qti.c',
    'vendor/ufs_hid_jedec.c': 'drivers/ufs/host/vendor/ufs_hid_jedec.c',
    'vendor/ufs_hid_jedec.h': 'drivers/ufs/host/vendor/ufs_hid_jedec.h',
    'include/linux/ipc_logging.h': 'include/linux/ipc_logging.h',
    'include/soc/qcom/minidump.h': 'include/soc/qcom/minidump.h',
    'include/ufs/ufshcd-crypto-qti.h': 'include/ufs/ufshcd-crypto-qti.h',
    'Makefile': None,
}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/ufs-host-source-lock.json'
    patch = ROOT / 'ports/ufs-host/port-7.3.patch'
    originals = {n: subprocess.check_output(['git', '-C', str(REPO), 'show', COMMIT + ':' + p]) if p else b''
                 for n, p in FILES.items()}
    if args.record:
        rows = []
        changes = ''
        for name, original in originals.items():
            path = 'ports/ufs-host/' + name
            data = (ROOT / path).read_bytes()
            rows.append({'file': path, 'source': FILES[name], 'original_sha256': sha(original), 'sha256': sha(data)})
            changes += ''.join(difflib.unified_diff(original.decode().splitlines(True), data.decode().splitlines(True),
                                                   fromfile='a/' + path, tofile='b/' + path))
        patch.write_text(changes)
        report = {'source_commit': COMMIT, 'files': rows, 'patch_sha256': sha(patch.read_bytes()),
                  'scope': 'source and reviewed patch replay only; final module link and physical acceptance pending',
                  'reviewed_changes': ['Explicit vendor ICE and ported CCF interfaces',
                                       'Private variant MCQ resource metadata instead of modifying core resource layout',
                                       '7.3 command hooks use scsi_cmnd and the actual request CPU/tag',
                                       'Move negotiation to negotiate_pwr_mode and preserve PRE/POST hardware updates',
                                       'HID support uses the real 7.3 device-descriptor bit',
                                       'Use native SCSI configure callback for broken FUA handling',
                                       'Map the old 5ms VCC-off delay to the actual 7.3 delay field',
                                       'Connect real QTI profile to CUSTOM_CRYPTO_PROFILE, propagate init failure',
                                       'Move raw key routine into profile module; HWKM rejects raw programming',
                                       'Use target MSI, thermal, removal, DME and tracing APIs'],
                  'known_open_items': ['Actual IPC/minidump providers before final link',
                                       'MCQ mapped resource bounds and managed inserted-resource cleanup',
                                       'ESI affinity bounds and CPU hotplug lifetime',
                                       'Host hook/notifier/sysfs/minidump registration lifetime',
                                       'Hardware training, reset, cryptographic profile compatibility and real I/O']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('UFS source/patch pin mismatch')
    if {r['file'] for r in report['files']} != {'ports/ufs-host/' + n for n in FILES}:
        raise ValueError('UFS source list mismatch')
    replay = ROOT / 'artifacts/ufs-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file'].removeprefix('ports/ufs-host/')
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('UFS source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('UFS patch replay mismatch')
    print('Verified in-progress UFS sources/replay:', len(FILES), 'files; link/hardware unproven')


if __name__ == '__main__':
    main()
