#!/usr/bin/env python3
"""Lock and replay the real SPMI/PMIC ports against pinned Motorola source."""
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
INPUTS = json.loads((ROOT / 'reports/spmi-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('Diagnostic source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-spmi/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/spmi-source-lock.json'
    patch = ROOT / 'patches/spmi-drivers-7.3.patch'
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
                      'Native fwnode IRQ domain API; preserve real V1 through V8 hardware operations',
                      'Managed debug/controller/IRQ teardown in dependency order, including failed probe',
                      'Hold bus OF references and bound the controller bus array',
                      'Preserve globally unique core controller names rather than hardware-local IDs',
                      'Publish only initialized arbiters; serialize mapping queries with unpublish',
                      'Balance mapping device/node references and validate address high bits',
                      'Fix debug-controller ERR_PTR allocation handling and iterator error cleanup',
                      'Keep PMIC revision metadata until managed children and debug resources leave',
                      'Track QTI owned dentries; full proxy and module owner for custom data access',
                      'Match actual devres release key for manual QTI unregister without recursive lock'],
                  'known_open_items': [
                      'Physical V8 APID/EE/ownership, secure accesses, transactions and interrupt delivery',
                      'Consumer ownership after receiving physical mapping resources',
                      'IRQ domain/mapping lifetime and concurrent PMIC probe/removal stress',
                      'PMIC metadata getter lifetime and QTI dump control/cache concurrency',
                      'Duplicate active SID4 in saved/running DT remains; candidate preserves running first selection',
                      'Native child enumeration is 16-SID; selected DT max SID13, broader eSPMI not validated']}

        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('Diagnostic source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('Diagnostic source list mismatch')
    replay = ROOT / 'artifacts/spmi-source-replay'
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
    print('Verified SPMI source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
