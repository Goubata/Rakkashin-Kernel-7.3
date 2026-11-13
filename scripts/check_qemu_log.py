#!/usr/bin/env python3
"""Require explicit runtime success; QEMU exit zero alone is insufficient."""
import argparse
import json
import re
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',type=Path)
p.add_argument('--test',choices=['smoke','clock','oplus','modules','iommu','secure','sysstats'],required=True)
p.add_argument("--clock-count",type=int,default=35)
p.add_argument("--module-count",type=int,default=20)
a=p.parse_args()
s=a.log.read_text(errors='replace')
markers={'smoke':'AVR_QEMU_SMOKE_PASS','clock':'AVR_QEMU_KUNIT_MODULES_LOADED','oplus':'AVR_OPLUS_SYNC_PASS','modules':f'AVR_VENDOR_MODULES_PASS count={a.module_count} ','iommu':'AVR_IOMMU_TESTS_LOADED','secure':'AVR_SECURE_TESTS_LOADED','sysstats':'AVR_SYSSTATS_TESTS_LOADED'}
failures=[]
for pattern in [r'Kernel panic',r'BUG:',r'WARNING:',r'\bnot ok\b',r'AVR_\w*FAIL',r'rcu: .*stall',r'rcu: .*detected stalls']:
    if re.search(pattern,s):failures.append(pattern)
if markers[a.test] not in s:failures.append('missing success marker')
if 'reboot: Power down' not in s:failures.append('guest did not shut down')
result={'log':str(a.log),'test':a.test,'passed':not failures,'failures':failures}
if a.test in ('clock','iommu','secure','sysstats'):
    expected=a.clock_count if a.test=='clock' else 5 if a.test=='secure' else 4 if a.test=='sysstats' else 3
    # KUnit omits Totals lines for single-case suites; count leaf KTAP
    # assertions at one indentation level, excluding suite result lines.
    passes=len(re.findall(r'\] {5}ok \d+ ',s))
    skips=len(re.findall(r'\] {5}ok \d+ .*# SKIP',s))
    result['totals']=[passes-skips,0,skips,passes]
    if result['totals'] != [expected,0,0,expected]:
        failures.append(f'expected all {expected} KTAP cases')
if a.test=='modules' and a.module_count>=20 and 'AVR_SYSSTATS_NETLINK_PASS checks=4' not in s:
    failures.append('missing sysstats ABI checks')
if a.test=='oplus':
    m=re.search(r'AVR_OPLUS_SYNC_PASS checks=(\d+)',s)
    result['checks']=int(m[1]) if m else 0
    if result['checks']<1283:failures.append('incomplete sync tests')
result['passed']=not failures
print(json.dumps(result,indent=2))
raise SystemExit(bool(failures))
