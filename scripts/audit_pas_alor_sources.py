#!/usr/bin/env python3
"""Lock and replay the native Alor PAS draft source and explicit port changes; physical firmware acceptance is separate."""
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
INPUTS = json.loads((ROOT / 'reports/pas-alor-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('Alor PAS draft source pin mismatch')
FILES = {row['file']: row['source'] for row in INPUTS['files']}
FILES.update({'ports/qcom-pas-alor/Makefile': None, 'ports/qcom-pas-alor/avr_pas_rails.h': None, 'ports/qcom-pas-alor/avr_pas_proxy.h': None})



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/pas-alor-source-lock.json'
    patch = ROOT / 'patches/pas-alor-7.3.patch'
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
                  'experimental_packaged': False,
                  'hardware_runtime_accepted': False,
                  'reviewed_changes': [
                      'Native PAS, PIL info and MDT loader sources with real explicit provider declarations',
                      'Alor-specific descriptor, ARC cx/mx votes with counted rollback and retained cleanup errors, ELF64 dump metadata; still excluded from artifacts',
                      'Native MDT parser validates nonnull inputs, ELF32/LE and bounded full header metadata before copying',
                      'Widen file/address additions before arithmetic and check metadata total size',
                      'Track acquired proxy resources, serialize handover, defer early handover until main authentication succeeds and retry incomplete release',
                  ],
                  'pending': ['Physical ARC votes, retained-error recovery and regulator framework fault injection',
                              'ELF64 minidump metadata and early-attach lifetime/failure cleanup',
                              'Full PAS clock/IRQ/handover/secure-error lifetimes and firmware-loader validation before packaging',
                              'Secure firmware ABI and physical all-functions acceptance']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('Alor PAS draft source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('Alor PAS draft source list mismatch')
    replay = ROOT / 'artifacts/pas-alor-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Alor PAS draft source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Alor PAS draft patch replay mismatch')
    print('Verified Alor PAS draft source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
