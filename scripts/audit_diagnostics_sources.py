#!/usr/bin/env python3
"""Lock and replay the real IPC/minidump ports against pinned Motorola source."""
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
INPUTS = json.loads((ROOT / 'reports/ufs-diagnostics-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('Diagnostic source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-minidump/Makefile': None, 'ports/qcom-ipc-logging/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/diagnostics-source-lock.json'
    patch = ROOT / 'patches/diagnostics-drivers-7.3.patch'
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
                  'reviewed_changes': [
                      'Actual native symbol lookup and native DMA buffer iterator with correct references',
                      'Keep target panic diagnostics; use native folio flags, CMA API and EEVDF counters',
                      'Bounded names and nonzero regions; cancel pending registrations and reject duplicates',
                      'Publish SMEM readiness inside the pending-transfer critical section',
                      'Track IPC regions, serialize 16-slot quota, unregister before freeing context/pages',
                      'Retain memory with an explicit error if the backend refuses unregistration',
                      'Saved AVR disables private ftrace capture; native oops output stays active'],
                  'known_open_items': [
                      'Physical SMEM/bootloader table and firmware dump acceptance',
                      'Active-backend registration/removal and failed-probe recovery tests',
                      'Panic path scheduler and memory metadata runtime review',
                      'IPC concurrent same-name context creation and reader teardown review',
                      'No IPC module unload: original module has no module_exit',
                      'Memory retained after failed unregister has no deferred reclamation yet']}

        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('Diagnostic source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('Diagnostic source list mismatch')
    replay = ROOT / 'artifacts/diagnostics-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Diagnostic source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Diagnostic patch replay mismatch')
    print('Verified diagnostic source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
