#!/usr/bin/env python3
"""Verify staged IPC sources against pinned Git blobs and explicit port changes."""
import hashlib
import difflib
import json
from pathlib import Path
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
AUDIT = ROOT.parent / 'avr-upstream-audit'
SOC = '037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
COMMON = '479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4'
NATIVE = '4dd306a07b2f456fd70ddaa28f4f65640374f202'

def git_blob(data):
    return hashlib.sha1(b'blob ' + str(len(data)).encode() + b'\0' + data).hexdigest()

def main():
    rows = []
    for path in sorted((ROOT / 'ports/gunyah').rglob('*')):
        if not path.is_file() or path.suffix not in ('.c', '.h') or path.name.endswith('.mod.c'):
            continue
        relative = path.relative_to(ROOT / 'ports/gunyah').as_posix()
        if relative in ('include/linux/gunyah.h', 'include/uapi/linux/gunyah.h'):
            repo, commit, origin = AUDIT / 'vendor-repos/kernel-common', COMMON, relative
        elif relative.startswith('include/asm/'):
            repo, commit, origin = AUDIT / 'qcom-soc', SOC, 'arch/arm64/' + relative
        elif relative.startswith('arm64/'):
            repo, commit, origin = AUDIT / 'qcom-soc', SOC, 'arch/arm64/gunyah/' + path.name
        elif relative.startswith('include/'):
            repo, commit, origin = AUDIT / 'qcom-soc', SOC, relative
        else:
            repo, commit, origin = AUDIT / 'qcom-soc', SOC, 'drivers/virt/gunyah/' + relative
        data = path.read_bytes()
        changes = []
        original = data
        if relative == 'gh_msgq.c':
            original = data.replace(b'static struct gh_msgq_cap_table *gh_msgq_alloc_entry(int label)',
                                    b'struct gh_msgq_cap_table *gh_msgq_alloc_entry(int label)', 1)
            changes.append('Scope file-local allocation helper static for target missing-prototype checking')
        if relative == 'arm64/reset.c':
            original = data + b'\n'
            changes.append('Remove upstream extra blank line at EOF')
        expected = subprocess.check_output(['git', '-C', str(repo), 'rev-parse', commit + ':' + origin], text=True).strip()
        if git_blob(original) != expected:
            raise ValueError('Unexpected IPC source modification: ' + relative)
        rows.append({'file': str(path.relative_to(ROOT)), 'source_repository': repo.name, 'commit': commit,
                     'origin': origin, 'original_git_blob': expected, 'port_changes': changes,
                     'sha256': hashlib.sha256(data).hexdigest()})
    for name in ('qcom_common.c', 'qcom_common.h'):
        path = ROOT / 'ports/qcom-rproc-common' / name
        origin = 'drivers/remoteproc/' + name
        expected = subprocess.check_output(['git', '-C', str(AUDIT / 'android-7.3'), 'rev-parse', NATIVE + ':' + origin], text=True).strip()
        data = path.read_bytes()
        original = data
        changes = []
        if name == 'qcom_common.c':
            for edge in ('glink', 'smd'):
                edit = ('\tif (!' + edge + '->edge)\n\t\treturn -EOPNOTSUPP;\n').encode()
                if original.count(edit) != 1:
                    raise ValueError('Missing or ambiguous transport error policy: ' + edge)
                original = original.replace(edit, b'', 1)
            changes.append('Require a real edge for selected GLINK/SMD subdevices; unavailable transport returns EOPNOTSUPP')
        if git_blob(original) != expected:
            raise ValueError('Native remoteproc helper differs from pinned source plus reviewed error policy: ' + name)
        if name == 'qcom_common.c':
            expected_patch = ''.join(difflib.unified_diff(original.decode().splitlines(True),
                data.decode().splitlines(True), fromfile='a/ports/qcom-rproc-common/qcom_common.c',
                tofile='b/ports/qcom-rproc-common/qcom_common.c')).encode()
            if (ROOT/'patches/rproc-transport-gate-7.3.patch').read_bytes() != expected_patch:
                raise ValueError('Remoteproc transport error policy patch differs from verified changes')
        rows.append({'file': str(path.relative_to(ROOT)), 'source_repository': 'android-7.3', 'commit': NATIVE,
                     'origin': origin, 'original_git_blob': expected, 'port_changes': changes,
                     'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    subprocess.run([sys.executable, str(ROOT/'scripts/audit_gunyah_core_sources.py'), '--vendor-rm'], check=True)
    vendor_lock = json.loads((ROOT/'reports/gunyah-rm-source-lock.json').read_text())
    for row in vendor_lock['files']:
        rows.append({'file': row['file'], 'source_repository': row['repository'],
                     'commit': row['commit'], 'origin': row['origin'],
                     'original_git_blob': row['original_git_blob'],
                     'port_changes': vendor_lock['reviewed_changes'], 'sha256': row['sha256']})
    report = {'passed': True, 'scope': 'staged source provenance only; not a runtime proof', 'remoteproc_build_sha256': hashlib.sha256((ROOT/'ports/qcom-rproc-common/Makefile').read_bytes()).hexdigest(), 'remoteproc_error_policy_patch_sha256': hashlib.sha256((ROOT/'patches/rproc-transport-gate-7.3.patch').read_bytes()).hexdigest(), 'files': rows}
    (ROOT / 'reports/ipc-source-provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Verified IPC source provenance:', len(rows), 'files')

if __name__ == '__main__':
    main()
