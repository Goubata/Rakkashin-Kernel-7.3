#!/usr/bin/env python3
"""Inventory source gaps; missing paths do not imply missing functionality."""
import json
from pathlib import Path
import subprocess
ROOT = Path(__file__).resolve().parents[1]
REF = ROOT.parent / 'avr-upstream-audit'

def tree(name):
    return set(subprocess.check_output(['git', '-C', str(REF/name), 'ls-tree', '-r', '--name-only', 'HEAD'], text=True).splitlines())

def main():
    soc, ack = tree('qcom-soc'), tree('android-7.3')
    candidates = sorted(p for p in soc if p.startswith(('drivers/', 'kernel/sched/'))
                        and (any(s in p for s in ['alor', 'canoe', '/walt/', 'vendor_hook'])
                             or p.endswith(('ufshcd-crypto-qti.c', 'arm-smmu-qcom.c'))))
    rows = [{'path': p, 'same_path_in_android73': p in ack} for p in candidates]
    modules = [l.strip() for l in (REF/'kernel-msm--modules-lists_modules.list.msm.canoe').read_text().splitlines()
               if l.strip() and not l.lstrip().startswith('#')]
    report = {'scope': 'Qualcomm canoe candidate list; not the list of loaded avr modules',
              'modules': modules, 'selected_soc_paths': rows,
              'warning': 'Path comparison only. Does not prove API compatibility or build success.'}
    (ROOT/'reports/port-inventory.json').write_text(json.dumps(report, indent=2)+'\n')
    print(f'{len(modules)} candidate modules; {len(rows)} selected SoC paths; '
          f'{sum(not r["same_path_in_android73"] for r in rows)} absent at same path in Android 7.3')

if __name__ == '__main__':
    main()
