#!/usr/bin/env python3
"""Lock and replay the real interconnect ports against pinned Motorola source."""
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
INPUTS = json.loads((ROOT / 'reports/interconnect-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('Diagnostic source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-interconnect/Makefile': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def load_original(name):
    row = next((r for r in INPUTS['files'] if r['file'] == name), None)
    if row is None:
        return b''
    pins = {'qcom-soc': (REPO, COMMIT),
            'kernel-common': (ROOT.parent/'avr-upstream-audit/vendor-repos/kernel-common',
                              '479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4')}
    repo, commit = pins[row['repo']]
    if row['commit'] != commit:
        raise ValueError('Interconnect source commit mismatch')
    data = subprocess.check_output(['git','-C',str(repo),'show',commit+':'+row['source']])
    if sha(data) != row['original_sha256']:
        raise ValueError('Original source digest mismatch')
    return data


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/interconnect-source-lock.json'
    patch = ROOT / 'patches/interconnect-drivers-7.3.patch'
    originals = {n: load_original(n) for n in FILES}
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
                  'additional_source_commit': '479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4',
                  'reviewed_changes': [
                      'Use exact five-bucket common binding header before native include guard',
                      'Preserve real RPMh/CRM voting and the unmodified Alor topology',
                      'Keep clock scaling local with 64-bit intermediates; propagate rate/commit failures',
                      'Validate full BCM auxiliary data and node capacity before publishing its address',
                      'Propagate BCM/link/debug registration failure instead of claiming probe success',
                      'Fix devm clock double release and map allocation failure handling',
                      'Remove children before the parent provider; manage populate rollback',
                      'Hold debug provider list during walks, put lookup dentry and drain trace unregister'],
                  'known_open_items': [
                      'Physical NoC/BCM/CRM/RPMh acknowledgement and actual bandwidth/power behavior',
                      'BCM static-state/probe rollback, removal and voter-handle lifetime races',
                      'QoS MMIO error propagation and dependency-vote rollback',
                      'Consumer-request list locking and concurrent debug trace controls',
                      'Alor and BCM voter have no original module_exit and are intentionally resident']}

        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('Diagnostic source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('Diagnostic source list mismatch')
    replay = ROOT / 'artifacts/interconnect-source-replay'
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
    print('Verified interconnect source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
