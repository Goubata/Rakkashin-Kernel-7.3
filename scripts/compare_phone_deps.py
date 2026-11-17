#!/usr/bin/env python3
"""Compare the depends= set of built port modules with the phone's vendor modules
(../avr-upstream-audit/phone-vendor-modules). Usage: compare_phone_deps.py [port ...]
(no args: all ports). Known renames are mapped (qcom_common -> rproc_qcom_common)."""
import subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PS = [ROOT.parent / 'avr-upstream-audit/phone-vendor-modules', ROOT.parent / 'avr-upstream-audit/phone-vendor-boot']
def phone(name):
    for d in PS:
        if (d / name).exists():
            return d / name
    return None
REN = {'qcom_common': 'rproc_qcom_common'}
def deps(k):
    for l in subprocess.run(['strings', str(k)], capture_output=True, text=True).stdout.splitlines():
        if l.startswith('depends='):
            return sorted(REN.get(x, x) for x in l[8:].split(',') if x)
    return []
ports = sys.argv[1:] or [p.name for p in (ROOT / 'ports').iterdir() if p.is_dir()] + ['@gki']
same = diff = 0
for port in ports:
    base = ROOT / 'out/native-gki' if port == '@gki' else ROOT / 'ports' / port
    for k in sorted(base.rglob('*.ko')):
        pk = phone(k.name)
        if pk is None:
            continue
        a, b = deps(k), deps(pk)
        if a == b:
            same += 1
        else:
            diff += 1
            print(f'DIFF {k.relative_to(ROOT)} missing={sorted(set(b)-set(a))} extra={sorted(set(a)-set(b))}')
print(f'same={same} diff={diff}')
