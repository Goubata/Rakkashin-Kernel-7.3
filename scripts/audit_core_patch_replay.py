#!/usr/bin/env python3
"""Replay the full native patch chain from the pinned base and compare exact files."""
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT.parent / 'avr-upstream-audit/android-7.3'
BASE = '4dd306a07b2f456fd70ddaa28f4f65640374f202'


def main():
    lock = json.loads((ROOT / 'reports/core-patch-checkpoint.json').read_text())
    paths = set()
    patches = []
    for name, digest in lock['patch_sha256'].items():
        patch = ROOT / name
        if hashlib.sha256(patch.read_bytes()).hexdigest() != digest:
            raise ValueError('Changed patch: ' + name)
        changed=set(re.findall(r'^(?:---|\+\+\+) [ab]/(\S+)', patch.read_text(), re.M))
        if not changed:raise ValueError('Patch has no recognized source paths: '+name)
        paths.update(changed)
        patches.append(patch)
    replay = ROOT / 'artifacts/core-patch-replay'
    if replay.exists():
        shutil.rmtree(replay)
    replay.mkdir(parents=True)
    subprocess.run(['git', 'init', '-q', str(replay)], check=True)
    for name in sorted(paths):
        original = subprocess.run(['git', '-C', str(SOURCE), 'show', BASE + ':' + name], capture_output=True)
        if original.returncode:
            exists = subprocess.run(['git', '-C', str(SOURCE), 'cat-file', '-e', BASE + ':' + name], capture_output=True)
            if exists.returncode != 128:
                raise ValueError('Unexpected base lookup failure: ' + name)
            continue
        path = replay / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(original.stdout)
    for patch in patches:
        subprocess.run(['git', '-C', str(replay), 'apply', str(patch)], check=True)
    rows = []
    for name in sorted(paths):
        target = subprocess.check_output(['git', '-C', str(SOURCE), 'show', lock['source_commit'] + ':' + name])
        if (replay / name).read_bytes() != target:
            raise ValueError('Replay differs from reviewed source: ' + name)
        rows.append({'file': name, 'sha256': hashlib.sha256(target).hexdigest()})
    report = {'scope': 'exact native source patch replay only; not clean build or hardware validation',
              'base_commit': BASE, 'target_commit': lock['source_commit'],
              'patch_count': len(patches), 'source_files': rows, 'passed': True,
              'checkpoint_sha256': hashlib.sha256((ROOT / 'reports/core-patch-checkpoint.json').read_bytes()).hexdigest()}
    (ROOT / 'reports/core-patch-replay.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Native patch replay:', len(patches), 'patches,', len(rows), 'exact files')


if __name__ == '__main__':
    main()
