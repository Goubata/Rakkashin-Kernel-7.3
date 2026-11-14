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

def original(relative, vendor_rm=False):
    if vendor_rm:
        soc_paths = {
            'gh_rm_core.c': 'drivers/virt/gunyah/gh_rm_core.c',
            'gh_rm_iface.c': 'drivers/virt/gunyah/gh_rm_iface.c',
            'gh_rm_drv_private.h': 'drivers/virt/gunyah/gh_rm_drv_private.h',
            'include/trace/events/gunyah.h': 'include/trace/events/gunyah.h',
            'include/linux/gunyah/gh_vm.h': 'include/linux/gunyah/gh_vm.h'}
        common_paths = {
            'include/drivers/virt/gunyah/rsc_mgr.h': 'drivers/virt/gunyah/rsc_mgr.h',
            'include/linux/gunyah_qtvm.h': 'include/linux/gunyah_qtvm.h'}
        if relative in ('avr-rm-deps.h', 'avr-rm-notifiers.h'):
            return {'repository': 'project', 'commit': None, 'origin': None}, b''
        if relative in soc_paths:
            repo = ROOT.parent / 'avr-upstream-audit/qcom-soc'
            commit = '037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
            origin = soc_paths[relative]
        else:
            repo, commit, origin = REPO, PIN, common_paths[relative]
        data = subprocess.check_output(['git', '-C', str(repo), 'show', commit + ':' + origin])
        return {'repository': repo.name, 'commit': commit, 'origin': origin}, data
    if relative == 'gunyah_hypercall.c':
        origin = 'arch/arm64/gunyah/gunyah_hypercall.c'
    elif relative.startswith('include/'):
        origin = relative
    else:
        origin = 'drivers/virt/gunyah/' + relative
    data = subprocess.check_output(['git', '-C', str(REPO), 'show', PIN + ':' + origin])
    return {'repository': REPO.name, 'commit': PIN, 'origin': origin}, data

def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--record', action='store_true', help='Record reviewed source edits and generate their replay patch')
    parser.add_argument('--vendor-rm',action='store_true',help='Audit the vendor RM adapter and owned notifier glue')
    args = parser.parse_args()
    directory = 'ports/gunyah-rm' if args.vendor_rm else 'ports/gunyah-core'
    lock_path = ROOT/'reports'/('gunyah-rm-source-lock.json' if args.vendor_rm else 'gunyah-core-source-lock.json')
    patch_path = ROOT/directory/'port-7.3.patch'
    files = sorted(p for p in (ROOT / directory).rglob('*')
                   if p.is_file() and p.suffix in ('.c', '.h') and not p.name.endswith('.mod.c'))
    originals = {}
    if args.record:
        rows = []
        patch = ''
        for path in files:
            relative = path.relative_to(ROOT / directory).as_posix()
            origin, data = original(relative, args.vendor_rm)
            originals[relative] = data
            current = path.read_bytes()
            label = directory + '/' + relative
            patch += ''.join(difflib.unified_diff(data.decode().splitlines(keepends=True),
                                                current.decode().splitlines(keepends=True),
                                                fromfile='a/' + label, tofile='b/' + label))
            rows.append({'file': label, **origin,
                         'original_git_blob': (hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest() if origin['origin'] else None),
                         'original_sha256': sha(data), 'sha256': sha(current), 'changed': current != data})
        patch_path.write_text(patch)
        report = {'kind': 'vendor-rm' if args.vendor_rm else 'common-core', 'scope': 'source provenance and patch replay only; not runtime or complete lifecycle validation',
                  'commit': PIN if not args.vendor_rm else None, 'patch_sha256': sha(patch_path.read_bytes()), 'files': rows,
                  'reviewed_changes': ['Target OF firmware-node conversion',
                                       'Target file mapping lookup and inclusive folio range',
                                       'Scope file-local VM failure helper static',
                                       'Physical PFN iteration for CMA pages',
                                       'Target void platform remove callback'] if not args.vendor_rm else [
                      'Target OF firmware-node and scoped DT references',
                      'Transactional SCM/hyp and RM/QTVM notifier registration',
                      'Probe failure and remove detach callbacks and drain work',
                      'Owned declaration/notifier lifecycle glue']}
        lock_path.write_text(json.dumps(report, indent=2) + '\n')
    report = json.loads(lock_path.read_text())
    if sha(patch_path.read_bytes()) != report['patch_sha256']:
        raise ValueError('Reviewed Gunyah replay patch changed')
    if {str(p.relative_to(ROOT)) for p in files} != {r['file'] for r in report['files']}:
        raise ValueError('Staged source list differs from lock')
    replay = ROOT / 'artifacts' / ('gunyah-rm-source-replay' if args.vendor_rm else 'gunyah-source-replay')
    if replay.exists():
        shutil.rmtree(replay)
    for row in report['files']:
        relative = Path(row['file']).relative_to(directory).as_posix()
        if relative not in originals:
            _, originals[relative] = original(relative, args.vendor_rm)
        data = originals[relative]
        if sha(data) != row['original_sha256'] or sha((ROOT / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Pinned or staged source changed: ' + row['file'])
        target = replay / row['file']
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)
    subprocess.run(['git', '-C', str(ROOT), 'apply', '--directory=' + replay.relative_to(ROOT).as_posix(), str(patch_path)], check=True)
    for row in report['files']:
        if sha((replay / row['file']).read_bytes()) != row['sha256']:
            raise ValueError('Patch replay differs: ' + row['file'])
    print(('Gunyah vendor RM' if args.vendor_rm else 'Gunyah common') + ' provenance and port patch replay verified:', len(report['files']), 'files')

if __name__ == '__main__':
    main()
