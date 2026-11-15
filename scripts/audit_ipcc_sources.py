#!/usr/bin/env python3
"""Lock and replay the native IPCC mailbox source and explicit port changes; physical firmware acceptance is separate."""
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
INPUTS = json.loads((ROOT / 'reports/ipcc-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('IPCC mailbox source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-ipcc/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/ipcc-source-lock.json'
    patch = ROOT / 'patches/ipcc-7.3.patch'
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
                      'Real native IPCC IRQ and mailbox provider with unloadable module registration',
                      'Reject client/signal truncation and reserved no-pending sentinel',
                      'Preallocate channel records; protect reservations and shutdown with a spinlock',
                      'Preserve duplicate checking past holes; avoid slots whose prior client is still bound',
                      'Use ordered devres: free IRQ, unregister mailbox, remove domain; no double unregister or unbalanced wake disable',
                      'Only release successfully parsed DT references',
                  ],
                  'pending': ['Physical IPCC IRQ/mailbox delivery and suspend/wakeup',
                              'Probe failure injection and live IRQ-domain consumer teardown',
                              'Whole-board DT/provider order and SoCCP integration']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('IPCC mailbox source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('IPCC mailbox source list mismatch')
    replay = ROOT / 'artifacts/ipcc-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('IPCC mailbox source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('IPCC mailbox patch replay mismatch')
    print('Verified IPCC mailbox source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
