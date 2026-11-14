#!/usr/bin/env python3
"""Lock and replay the real regulator ports against pinned Motorola source."""
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
INPUTS = json.loads((ROOT / 'reports/regulators-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('Diagnostic source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-regulators/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/regulators-source-lock.json'
    patch = ROOT / 'patches/regulators-drivers-7.3.patch'
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
                      'Preserve firmware voltage/mode/RPMh aggregation and real IPC/debug providers',
                      'Hold OF nodes with managed references and drop iterator on errors',
                      'Propagate proxy-consumer failure and manage populated child devices',
                      'Rollback failed driver registration and destroy IPC log after unregister',
                      'Track only owned debugfs files, drain accesses before freeing private data',
                      'Use full debugfs proxy for custom voltage operations and module ownership',
                      'Serialize lazy diagnostic consumer acquisition and synchronize trace removal'],
                  'known_open_items': [
                      'Physical RPMh acknowledgements, rail votes, suspend/resume and voltage transitions',
                      'Review aggregate request rollback and acknowledgement/error paths',
                      'Concurrent debugfs removal, regulator core locks and suspend diagnostics stress',
                      'Debug consumer holds provider module reference until device unbind',
                      'Optional suspend-debug exports are absent from the saved AVR module; see saved feature report']}

        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('Diagnostic source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('Diagnostic source list mismatch')
    replay = ROOT / 'artifacts/regulators-source-replay'
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
    print('Verified regulator source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
