#!/usr/bin/env python3
"""Lock and replay the native moto-info source and explicit port changes; physical firmware acceptance is separate."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[1]
REPO = ROOT.parent / 'avr-upstream-audit/vendor-repos/motorola-kernel-modules'
COMMIT = 'a24357771fe451d829a2f78f8986ecd53522085e'
INPUTS = json.loads((ROOT / 'reports/moto-info-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('moto-info source pin mismatch')
FILES = INPUTS['files']



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/moto-info-source-lock.json'
    patch = ROOT / 'patches/moto-info-7.3.patch'
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
                  'module_packaging': {'mmi_annotate': 'partial snapshot; physical backend acceptance pending', 'mmi_info': 'partial snapshot; physical SMEM/ROM acceptance pending'},
                  'hardware_runtime_accepted': False,
                  'reviewed_changes': ['Add exact missing factory headers and required exported prototype; make private build-signature parser static', 'Use real SMEM API and validate allocation/get errors and exact region size before copying', 'Bound annotation formatting to actual emitted bytes, subtraction bounds and explicit NUL termination', 'Treat boot signature strings as data rather than printf formats; preserve public headers and metadata endpoint names/modes', 'Initialize owned annotate state before publication; bound restore using resource capacity; use memremap/memunmap for no-map RAM', 'Serialize API/remove; unwind proc/map/buffer acquisitions; remove minidump registration before backing release', 'Retain unconfirmed minidump backing and retry at next probe/exit; terminal failure intentionally retains backing until reboot', 'Keep immutable exact-key bootarg values with native cmdline parsing and bounded bootconfig/OF string handling', 'Own DDR snapshots; validate SMEM pointers and lengths before reads; retain unit-info v3/1024-byte payload', 'Use independently probed software metadata endpoints with real deferred probe and reverse registration unwind; drain proc/sysfs before freeing backing'],
                  'pending': ['Annotation physical minidump backend/cache/boot acceptance and allocation/proc failure injection remain pending', 'Info physical initialized-SMEM/modem acceptance, automatic supplier readiness and complete ROM behavior; low-memory allocation injection pending', 'Actual firmware/userspace/physical device acceptance']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('moto-info source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('moto-info source list mismatch')
    replay = ROOT / 'artifacts/moto-info-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('moto-info source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('moto-info patch replay mismatch')
    print('Verified moto-info source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
