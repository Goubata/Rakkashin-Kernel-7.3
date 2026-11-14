#!/usr/bin/env python3
"""Lock and replay the native GLINK transport source and explicit port changes; physical firmware acceptance is separate."""
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
INPUTS = json.loads((ROOT / 'reports/glink-transport-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('GLINK transport source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-glink-transport/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/glink-transport-source-lock.json'
    patch = ROOT / 'patches/glink-transport-7.3.patch'
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
                      'Real external native GLINK/SMEM/control modules using current RPMSG internals and real SMEM exports',
                      'Preserve version 1 wire IDs, feature negotiation, ring barriers and READ_NOTIF reserve',
                      'Validate FIFO geometry, indices and bounded peek/advance/write without touching RAM on invalid calls',
                      'Publish RX FIFO length before its pointer and use single-copy shared head/tail accesses',
                      'Reject invalid registration arguments, retain OF nodes and propagate deferred IRQ mapping',
                      'Use native explicit per-CPU workqueues and reject oversized TX sizes before integer narrowing',
                      'Abort/wake blocked TX before cancelling response work; wait predicates observe abort and no post-abort write',
                      'Load real RPMSG control driver before GLINK via soft dependency',
                  ],
                  'pending': [
                      'Physical SMEM partition 19, descriptor/IRQ/mailbox/coherency and remote SoCCP firmware negotiation',
                      'Full intents/data/channel/SSR protocol and callback lifetime stress with remote endpoint',
                      'Registration/probe fault injection, long ordinary TX backpressure and RX malformed-frame recovery',
                      'Qualcomm vendor extensions beyond native wire features and saved module behavior equivalence',
                      'SoCCP/PAS/SMEM state/SMP2P/mailbox and all phone/charging/USB child service acceptance',
                  ]}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('GLINK transport source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('GLINK transport source list mismatch')
    replay = ROOT / 'artifacts/glink-transport-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('GLINK transport source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('GLINK transport patch replay mismatch')
    print('Verified GLINK transport source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
