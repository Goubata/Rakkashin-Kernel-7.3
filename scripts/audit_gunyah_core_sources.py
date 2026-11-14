#!/usr/bin/env python3
"""Lock/replay Gunyah common sources and the reviewed port patch against pinned blobs."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent / 'avr-upstream-audit/vendor-repos/kernel-common'
PIN = '479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4'
LOCK = ROOT / 'reports/gunyah-core-source-lock.json'
PATCH = ROOT / 'ports/gunyah-core/port-7.3.patch'

def sha(data):
    return hashlib.sha256(data).hexdigest()

def original(relative):
    if relative == 'gunyah_hypercall.c':
        origin = 'arch/arm64/gunyah/gunyah_hypercall.c'
    elif relative.startswith('include/'):
        origin = relative
    else:
        origin = 'drivers/virt/gunyah/' + relative
    data = subprocess.check_output(['git', '-C', str(REPO), 'show', PIN + ':' + origin])
    return origin, data

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true', help='Record reviewed source edits and generate their replay patch')
    args = parser.parse_args()
    files = sorted(p for p in (ROOT / 'ports/gunyah-core').rglob('*')
                   if p.is_file() and p.suffix in ('.c', '.h') and not p.name.endswith('.mod.c'))
    originals = {}
    if args.record:
        rows = []
        patch = ''
        for path in files:
            relative = path.relative_to(ROOT / 'ports/gunyah-core').as_posix()
            origin, data = original(relative)
            originals[relative] = data
            current = path.read_bytes()
            label = 'ports/gunyah-core/' + relative
            patch += ''.join(difflib.unified_diff(data.decode().splitlines(keepends=True),
                                                current.decode().splitlines(keepends=True),
                                                fromfile='a/' + label, tofile='b/' + label))
            rows.append({'file': label, 'origin': origin,
                         'original_git_blob': hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest(),
                         'original_sha256': sha(data), 'sha256': sha(current), 'changed': current != data})
        PATCH.write_text(patch)
        report = {'scope': 'source provenance and patch replay only; not runtime or complete lifecycle validation',
                  'commit': PIN, 'patch_sha256': sha(PATCH.read_bytes()), 'files': rows,
                  'reviewed_changes': ['Target OF firmware-node conversion',
                                       'Target file mapping lookup and inclusive folio range',
                                       'Scope file-local VM failure helper static',
                                       'Physical PFN iteration for CMA pages',
                                       'Target void platform remove callback']}
        LOCK.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(LOCK.read_text())
    if sha(PATCH.read_bytes()) != report['patch_sha256']:
        raise ValueError('Reviewed Gunyah replay patch changed')
    if {str(p.relative_to(ROOT)) for p in files} != {r['file'] for r in report['files']}:
        raise ValueError('Staged source list differs from lock')
    replay = ROOT / 'artifacts/gunyah-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        relative = Path(row['file']).relative_to('ports/gunyah-core').as_posix()
        if relative not in originals:
            _, originals[relative] = original(relative)
        data = originals[relative]
        if sha(data) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Pinned or staged source changed: ' + row['file'])
        target = replay / row['file']
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=artifacts/gunyah-source-replay', str(PATCH)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Patch replay differs: ' + row['file'])
    print('Gunyah common provenance and port patch replay verified:', len(report['files']), 'files')

if __name__ == '__main__':
    main()
