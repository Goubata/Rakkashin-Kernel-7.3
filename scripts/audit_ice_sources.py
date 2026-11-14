#!/usr/bin/env python3
"""Lock and replay reviewed ICE changes over pinned Motorola source files."""
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
FILES = {'ice.c': 'drivers/soc/qcom/ice.c', 'include/soc/qcom/ice.h': 'include/soc/qcom/ice.h',
         'avr-ice-policy.h': None, 'Makefile': None}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    patch = ROOT / 'ports/qcom-ice/port-7.3.patch'
    lock = ROOT / 'reports/ice-source-lock.json'
    originals = {name: subprocess.check_output(['git', '-C', str(REPO), 'show', COMMIT + ':' + source]) if source else b''
                 for name, source in FILES.items()}
    if args.record:
        rows = []
        changes = ''
        for name, original in originals.items():
            path = 'ports/qcom-ice/' + name
            data = (ROOT / path).read_bytes()
            rows.append({'file': path, 'source': FILES[name],
                         'source_repository': 'qcom-soc' if FILES[name] else 'avr-kernel-7.3',
                         'original_sha256': sha(original), 'sha256': sha(data)})
            changes += ''.join(difflib.unified_diff(original.decode().splitlines(True), data.decode().splitlines(True),
                                                   fromfile='a/' + path, tofile='b/' + path))
        patch.write_text(changes)
        report = {'source_commit': COMMIT, 'files': rows, 'patch_sha256': sha(patch.read_bytes()),
                  'reviewed_changes': ['Real V2 SCM commands and target TZMEM allocation instead of cached qtee buffers',
                                       'Target blk_crypto_key bytes field and private vendor ICE header',
                                       'Keep HWKM version-dependent slot mapping with overflow/negative guards',
                                       'Reject unexpected raw requests in wrapped-key programming entry',
                                       'Reject incomplete HWKM initialization before wrapped-key MMIO',
                                       'Return BIST errors without silently downgrading HW-wrapped mode to raw',
                                       'Check BIST before marking resume ready and roll back newly enabled clock on failure'],
                  'scope': 'source identity and patch replay, not real key handling or storage acceptance'}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('ICE source pin/patch mismatch')
    if {row['file'] for row in report['files']} != {'ports/qcom-ice/' + n for n in FILES}:
        raise ValueError('ICE staged source list mismatch')
    replay = ROOT / 'artifacts/ice-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file'].removeprefix('ports/qcom-ice/')
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('ICE staged/original source mismatch: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('ICE patch replay mismatch')
    print('Verified ICE source provenance and port replay:', len(FILES), 'files')


if __name__ == '__main__':
    main()
