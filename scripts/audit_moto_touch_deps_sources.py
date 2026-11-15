#!/usr/bin/env python3
"""Lock and replay the native Motorola touch dependencies source and explicit port changes; physical firmware acceptance is separate."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent / 'avr-upstream-audit/vendor-repos/motorola-kernel-modules'
COMMIT = 'a24357771fe451d829a2f78f8986ecd53522085e'
INPUTS = json.loads((ROOT / 'reports/moto-touch-deps-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('Motorola touch dependencies source pin mismatch')
FILES = INPUTS['files']



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/moto-touch-deps-source-lock.json'
    patch = ROOT / 'patches/moto-touch-deps-7.3.patch'
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
                  'reviewed_changes': ['Factory Motorola relay and sensors class with selected API/lifetime checks; partial packaging only', 'Publish fully initialized singleton relay, SRCU-unpublish/drain on remove, drain absent-provider unregister', 'Use real notifier head locking without global mutex held across callbacks; validate enum and notifier inputs', 'Use atomic device attribute groups for sensor class registration with framework rollback on failure; reject missing names and clear device after unregister; factory names/modes preserved'],
                  'pending': ['Full consumer recursion constraints, reprobe coordination and sensor sysfs callbacks', 'Actual OF binding, touch/gesture/ROM integration and physical acceptance']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('Motorola touch dependencies source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('Motorola touch dependencies source list mismatch')
    replay = ROOT / 'artifacts/moto-touch-deps-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Motorola touch dependencies source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Motorola touch dependencies patch replay mismatch')
    print('Verified Motorola touch dependencies source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
