#!/usr/bin/env python3
"""Verify staged IPC sources against pinned Git blobs and explicit port changes."""
import hashlib
import json
from pathlib import Path
import subprocess

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
        if git_blob(path.read_bytes()) != expected:
            raise ValueError('Native remoteproc helper differs from pinned source: ' + name)
        rows.append({'file': str(path.relative_to(ROOT)), 'source_repository': 'android-7.3', 'commit': NATIVE,
                     'origin': origin, 'original_git_blob': expected, 'port_changes': [],
                     'sha256': hashlib.sha256(path.read_bytes()).hexdigest()})
    for relative, origin, use_common in [
        ('gh_rm_core.c','drivers/virt/gunyah/gh_rm_core.c',False),
        ('gh_rm_iface.c','drivers/virt/gunyah/gh_rm_iface.c',False),
        ('gh_rm_drv_private.h','drivers/virt/gunyah/gh_rm_drv_private.h',False),
        ('include/trace/events/gunyah.h','include/trace/events/gunyah.h',False),
        ('include/linux/gunyah/gh_vm.h','include/linux/gunyah/gh_vm.h',False),
        ('include/drivers/virt/gunyah/rsc_mgr.h','drivers/virt/gunyah/rsc_mgr.h',True)]:
        path=ROOT/'ports/gunyah-rm'/relative
        data=path.read_bytes();original=data;changes=[]
        if relative=='gh_rm_core.c':
            original=original.replace(b'of_fwnode_handle(gh_rm_intc)',b'of_node_to_fwnode(gh_rm_intc)')
            changes.append('Use target OF firmware-node conversion API')
        if relative=='include/linux/gunyah/gh_vm.h':
            original=original.replace(b'#ifdef CONFIG_QTVM_WITH_AVF\n#include <linux/gunyah_qtvm.h>\n#endif',b'#include <linux/gunyah_qtvm.h>')
            changes.append('Include QTVM-only header only for the QTVM build path')
        repo=AUDIT/('vendor-repos/kernel-common' if use_common else 'qcom-soc')
        commit=COMMON if use_common else SOC
        expected=subprocess.check_output(['git','-C',str(repo),'rev-parse',commit+':'+origin],text=True).strip()
        if git_blob(original)!=expected:raise ValueError('Unexpected RM modification: '+relative)
        rows.append({'file':str(path.relative_to(ROOT)),'source_repository':repo.name,'commit':commit,
                     'origin':origin,'original_git_blob':expected,'port_changes':changes,
                     'sha256':hashlib.sha256(data).hexdigest()})
    report = {'passed': True, 'scope': 'staged source provenance only; not a runtime proof', 'files': rows}
    (ROOT / 'reports/ipc-source-provenance.json').write_text(json.dumps(report, indent=2) + '\n')
    print('Verified IPC source provenance:', len(rows), 'files')

if __name__ == '__main__':
    main()
