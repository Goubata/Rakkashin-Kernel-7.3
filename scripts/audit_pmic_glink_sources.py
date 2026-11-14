#!/usr/bin/env python3
"""Lock and replay the draft firmware PMIC GLINK source; this does not accept its runtime lifecycle against pinned Motorola source."""
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
INPUTS = json.loads((ROOT / 'reports/pmic-glink-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('PMIC GLINK draft source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-pmic-glink/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/pmic-glink-source-lock.json'
    patch = ROOT / 'patches/pmic-glink-drivers-7.3.patch'
    originals = {n: subprocess.check_output(['git', '-C', str(REPO), 'show', COMMIT + ':' + p]) if p else b''
                 for n, p in FILES.items()}
    if args.record:
        rows = []
        changes = ''
        for name, original in originals.items():
            path = name
            data = (ROOT / path).read_bytes()
            rows.append({'file': path, 'source': FILES[name], 'original_sha256': sha(original), 'sha256': sha(data)})
            changes += ''.join(difflib.unified_diff(original.decode().splitlines(True), data.decode().splitlines(True),
                                                   fromfile='a/' + path, tofile='b/' + path))
        patch.write_text(changes)
        report = {'source_commit': COMMIT, 'files': rows, 'patch_sha256': sha(patch.read_bytes()),
                  'scope': 'source and reviewed patch replay only; module link is recorded separately; physical acceptance pending',
                  'draft_not_packaged': True,
                  'reviewed_changes': [
                      'Explicit native OF header and real SSR/PDR/IPC imports',
                      'Validate message data, minimum header length and integer length before sending',
                      'Validate client-data pointer and representable nonzero IDR IDs',
                      'Reject unterminated rpmsg channel name length and initialize device before notifier registration',
                      'Unregister platform driver if rpmsg registration fails',
                      'Acquire per-client SRCU while holding the lookup mutex and drain it before client free',
                      'Serialize RX/state callbacks outside the registry mutex and reject self-unregister with EDEADLK',
                      'Initialize and unwind each client SRCU before/after registry publication',
                      'Protect RPMSG driver-data ingress with module-lifetime SRCU until work is queued',
                      'Unpublish transport, drain ingress, cancel init work and drain RX before RPMSG removal completes',
                      'Retain supplier devices, verify binding under device lock, and link RPMSG consumers before transport publication',
                      'Protect client registration with the provider registry epoch and drain before child teardown',
                      'Unpublish provider/cancel init, stop notifiers, drain RX, remove children, then destroy IDR and IPC last',
                  ],
                  'pending': [
                      'Provider/rpmsg/global registry references and unbind serialization',
                      'Remove/init work ordering and child/notifier/IPC/IDR cleanup',
                      'Broader callback/register/unregister churn and all remote firmware/charging acceptance',
                      'Callers must stop concurrent TX before unregister and schedule unregister outside callbacks',
                  ]}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('PMIC GLINK draft source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('PMIC GLINK draft source list mismatch')
    replay = ROOT / 'artifacts/pmic-glink-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('PMIC GLINK draft source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('PMIC GLINK draft patch replay mismatch')
    print('Verified PMIC GLINK draft source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
