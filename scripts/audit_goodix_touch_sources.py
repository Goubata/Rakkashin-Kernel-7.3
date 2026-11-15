#!/usr/bin/env python3
"""Lock and replay the native goodix-touch source and explicit port changes; physical firmware acceptance is separate."""
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
INPUTS = json.loads((ROOT / 'reports/goodix-touch-source-inputs.json').read_text())
if INPUTS['source_commit'] != COMMIT:
    raise ValueError('goodix-touch source pin mismatch')
FILES = INPUTS['files']



def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true')
    args = parser.parse_args()
    lock = ROOT / 'reports/goodix-touch-source-lock.json'
    patch = ROOT / 'patches/goodix-touch-7.3.patch'
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
                  'reviewed_changes': ['Preserve all nine factory build flags and header UAPI; no dummy dependencies', 'Replace removed strncpy for strings, bind trace header to external build path, declare TUI exports and private helpers', 'Bound fixed custom-info copy to the actual ten-byte protocol length', 'Remove input_free_device after input_unregister_device to avoid pen double free', 'Preserve stock legacy GET_EVENTS no-payload return behavior, confirmed from archived instructions; draft remains unaccepted', 'Bound multi-SPI descriptor count and aggregate bytes to factory 5120-byte capacity before payload access; use kcalloc and negative allocation/uaccess errors', 'Propagate wakeup speed/SPI/restore failures while always attempting speed restore; stop receive copies at first fault', 'Bound frame payloads to slot capacity, reject invalid ring indices, use atomic request IDs and zero-initialize frame ioctl output', 'Bound TSC advertised length and version destination with NUL validation; clear old version tail and use bounded logging', 'Allocate zeroed dedicated pages with failure checking; validate mmap offset and length before shift; insert refcounted pages and detach owner under frame mutex'],
                  'pending': ['Open/ioctl/remove lifetime, partial probe unwind, frame bounds and concurrency review/tests', 'Real SPI/IRQ/display/power/gesture/stylus firmware and daemon integration', 'Actual firmware/userspace/physical device acceptance']}
        lock.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock.read_text())
    if report['source_commit'] != COMMIT or report['patch_sha256'] != sha(patch.read_bytes()):
        raise ValueError('goodix-touch source/patch pin mismatch')
    if {r['file'] for r in report['files']} != set(FILES):
        raise ValueError('goodix-touch source list mismatch')
    replay = ROOT / 'artifacts/goodix-touch-source-replay'
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        name = row['file']
        if sha(originals[name]) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('goodix-touch source changed: ' + name)
        path = replay / row['file']
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(originals[name])
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('goodix-touch patch replay mismatch')
    print('Verified goodix-touch source/patch replay:', len(FILES), 'files; hardware acceptance separate')


if __name__ == '__main__':
    main()
