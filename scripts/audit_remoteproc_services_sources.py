#!/usr/bin/env python3
"""Lock and replay the native remoteproc services source and explicit port changes; physical firmware acceptance is separate."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent / 'avr-upstream-audit/android-7.3'
COMMIT = '4dd306a07b2f456fd70ddaa28f4f65640374f202'
INPUTS = json.loads((ROOT / 'reports/remoteproc-services-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('remoteproc services source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-remoteproc-services/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/remoteproc-services-source-lock.json'
    patch = ROOT / 'patches/remoteproc-services-7.3.patch'
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
                  'experimental_packaged': True,
                  'hardware_runtime_accepted': False,
                  'reviewed_changes': [
                      'Real native Q6V5 common, sysmon/QMI and AOSS QMP modules; force real API declarations',
                      'Free managed sysmon IRQ before manually freeing callback data on QMI failure/removal',
                      'Missing optional shutdown IRQ reports false without querying invalid IRQ or warning',
                      'Drain successful Q6V5 IRQ registrations on explicit deinit and release QMP on ICC acquisition failure',
                      'Bound firmware crash reason string reads to actual SMEM size',
                      'Propagate failed stop-state assertion/clear instead of treating requests as successful',
                      'Serialize RPMSG association with sysmon list/removal and request mutex; unpublish and SRCU-drain callbacks before provider free',
                      'Read current endpoint association inside SRCU instead of stale callback priv; reject duplicate channel association',
                  ],
                  'pending': ['Full RPMSG transport/endpoint lifecycle and concurrent callback/provider removal stress',
                              'Real IRQ/probe failure injection and remote QMI/QMP peer protocol',
                              'Alor PAS adapter, rails, firmware and physical early attach']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('remoteproc services source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('remoteproc services source list mismatch')
    replay = ROOT / 'artifacts/remoteproc-services-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('remoteproc services source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('remoteproc services patch replay mismatch')
    print('Verified remoteproc services source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
