#!/usr/bin/env python3
"""Lock and replay the native power key source and explicit port changes; physical firmware acceptance is separate."""
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
INPUTS = json.loads((ROOT / 'reports/power-key-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('power key source pin mismatch')
FILES = INPUTS['files']



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/power-key-source-lock.json'
    patch = ROOT / 'patches/power-key-7.3.patch'
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
                  'reviewed_changes': ['Use native input, PON parent and real reboot-mode provider', 'Preserve observed stock Gen3 resin wakeup enabled default'],
                  'pending': ['Actual PMIC probe/IRQ/wake, ROM input interfaces and full suspend acceptance']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('power key source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('power key source list mismatch')
    replay = ROOT / 'artifacts/power-key-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('power key source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('power key patch replay mismatch')
    print('Verified power key source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
