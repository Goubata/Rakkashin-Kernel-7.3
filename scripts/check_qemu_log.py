#!/usr/bin/env python3
"""Require explicit runtime success; QEMU exit zero alone is insufficient."""
import argparse
import json
import re
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',type=Path)
p.add_argument('--test',choices=['remoteproc-services','smem-state','smp2p','ipcc','glink-transport','rproc-gate','pmic-glink','pmic-ipc','adc-thermal','pmic-gpio','spmi','interconnect','regulators','storage-host','diagnostics','debug-symbol','ufs','ice','storage-phy','smoke','clock','oplus','modules','iommu','secure','sysstats','fence','rakkashin','ntsync','power','adios','platform','scm','gpu-policy','rm-lifetime'],required=True)
p.add_argument("--clock-count",type=int,default=35)
p.add_argument("--module-count",type=int,default=20)
a=p.parse_args()
s=a.log.read_text(errors='replace')
markers={'remoteproc-services':'AVR_REMOTEPROC_SERVICES_TESTS_PASS scope=native-callbacks-local-qmi-and-driver-registration-only','smem-state':'AVR_SMEM_STATE_TESTS_PASS scope=native-state-api-and-concurrent-teardown-only','smp2p':'AVR_SMP2P_TESTS_PASS scope=ram-state-and-driver-registration-only','ipcc':'AVR_IPCC_TESTS_PASS scope=ram-registers-and-driver-registration-only','glink-transport':'AVR_GLINK_TRANSPORT_TESTS_PASS scope=ram-ring-wire-and-driver-registration-only','rproc-gate':'AVR_RPROC_GATE_TESTS_PASS scope=missing-or-invalid-transport-error-only','pmic-glink':'AVR_PMIC_GLINK_TESTS_PASS scope=source-callbacks-and-native-local-ipc-only','pmic-ipc':'AVR_PMIC_IPC_TESTS_PASS scope=native-local-ipc-only','adc-thermal':'AVR_ADC_THERMAL_TESTS_PASS scope=conversion-and-module-lifecycle-only','pmic-gpio':'AVR_PMIC_GPIO_TESTS_PASS scope=callback-source-on-ram-only','spmi':'AVR_SPMI_TESTS_PASS scope=ram-regmap-and-native-framework-only','interconnect':'AVR_INTERCONNECT_TESTS_PASS scope=aggregation-and-module-lifecycle-only','regulators':'AVR_REGULATORS_TESTS_PASS scope=logical-rail-and-teardown-only','storage-host':'AVR_STORAGE_HOST_TESTS_PASS scope=module-lifecycle-only','diagnostics':'AVR_DIAGNOSTICS_TESTS_PASS','debug-symbol':'AVR_DEBUG_SYMBOL_TESTS_PASS','ufs':'AVR_UFS_TESTS_PASS','ice':'AVR_ICE_TESTS_PASS','storage-phy':'AVR_STORAGE_PHY_TESTS_PASS count=2','rm-lifetime':'AVR_RM_LIFETIME_TESTS_PASS','gpu-policy':'AVR_GPU_POLICY_TESTS_PASS','scm':'AVR_SCM_TESTS_PASS','platform':'AVR_PLATFORM_TESTS_PASS','smoke':'AVR_QEMU_SMOKE_PASS','clock':'AVR_QEMU_KUNIT_MODULES_LOADED','oplus':'AVR_OPLUS_SYNC_PASS','modules':f'AVR_VENDOR_MODULES_PASS count={a.module_count} ','iommu':'AVR_IOMMU_TESTS_LOADED','secure':'AVR_SECURE_TESTS_LOADED','sysstats':'AVR_SYSSTATS_TESTS_LOADED','fence':'AVR_FENCE_TESTS_LOADED','rakkashin':'AVR_RAKKASHIN_PASS','ntsync':'AVR_NTSYNC_SELFTEST_PASS','power':'AVR_POWER_TESTS_LOADED','adios':'AVR_ADIOS_TESTS_PASS rounds=3 workers=4'}
failures=[]
for pattern in [r'Kernel panic',r'BUG:',r'WARNING:',r'\bnot ok\b',r'AVR_\w*FAIL',r'rcu: .*stall',r'rcu: .*detected stalls']:
    if re.search(pattern,s):failures.append(pattern)
if markers[a.test] not in s:failures.append('missing success marker')
if 'reboot: Power down' not in s:failures.append('guest did not shut down')
result={'log':str(a.log),'test':a.test,'passed':not failures,'failures':failures}
if a.test in ('remoteproc-services','smem-state','smp2p','ipcc','glink-transport','rproc-gate','pmic-glink','pmic-ipc','adc-thermal','pmic-gpio','spmi','interconnect','regulators','storage-host','diagnostics','debug-symbol','ufs','ice','clock','iommu','secure','sysstats','fence','power','platform','scm','gpu-policy','rm-lifetime'):
    expected=11 if a.test=='remoteproc-services' else 3 if a.test=='smem-state' else 8 if a.test=='smp2p' else 7 if a.test=='ipcc' else 11 if a.test=='glink-transport' else 2 if a.test=='rproc-gate' else 19 if a.test=='pmic-glink' else 6 if a.test=='pmic-ipc' else 24 if a.test=='adc-thermal' else 7 if a.test=='pmic-gpio' else 5 if a.test=='spmi' else 9 if a.test=='interconnect' else 7 if a.test=='regulators' else 8 if a.test in ('storage-host','diagnostics') else 5 if a.test=='debug-symbol' else 3 if a.test=='ufs' else 10 if a.test=='ice' else 9 if a.test=='rm-lifetime' else 8 if a.test=='gpu-policy' else 13 if a.test=='scm' else 10 if a.test=='platform' else a.clock_count if a.test=='clock' else 5 if a.test in ('secure','fence','power') else 4 if a.test=='sysstats' else 3
    # KUnit omits Totals lines for single-case suites; count leaf KTAP
    # assertions at one indentation level, excluding suite result lines.
    passes=len(re.findall(r'\] {5}ok \d+ ',s))
    skips=len(re.findall(r'\] {5}ok \d+ .*# SKIP',s))
    result['totals']=[passes-skips,0,skips,passes]
    if result['totals'] != [expected,0,0,expected]:
        failures.append(f'expected all {expected} KTAP cases')
if a.test in ('pmic-ipc','pmic-glink') and 'AVR_QRTR_LOCAL_PASS packets=128 scope=native-local-sockets-only' not in s:
    failures.append('missing actual QRTR local socket exchange')
if a.test=='pmic-glink' and 'AVR_PMIC_GLINK_REAL_DRIVER_LIFECYCLE_PASS' not in s:
    failures.append('missing production driver registration/unregistration check')
if a.test=='ufs' and 'AVR_UFS_IO_PASS rounds=2 bytes_written=8388608 bytes_read=8388608' not in s:
    failures.append('missing real emulated-UFS direct I/O checks')
if a.test=='storage-phy' and 'AVR_STORAGE_PHY_LOADED count=2' not in s:
    failures.append('missing both real UFS PHY module loads')
if a.test=='platform' and 'AVR_GUNYAH_GUEST_GATE_PASS' not in s:
    failures.append('missing real Gunyah guest rejection check')
if a.test=='modules' and a.module_count>=20 and 'AVR_SYSSTATS_NETLINK_PASS checks=4' not in s:
    failures.append('missing sysstats ABI checks')
if a.test=='adios':
    for seed in (1,2,3):
        if f'AVR_ADIOS_IO_PASS seed={seed} bytes_written=2097152 workers=4' not in s:failures.append(f'missing ADIOS round {seed}')
if a.test=='rakkashin':
    match=re.search(r'AVR_RAKKASHIN_PASS checks=(\d+)',s)
    result['checks']=int(match[1]) if match else 0
    if result['checks']<100:failures.append('incomplete Rakkashin tests')
    for marker in ['AVR_APP_PROFILE_PASS','AVR_THREAD_BOOST_PASS','AVR_LAUNCH_BOOST_PASS checks=4']:
        if marker not in s:failures.append('missing '+marker)
if a.test=='ntsync':
    result['tap_passes']=len(re.findall(r'^ok \d+ ',s,re.M))
    result['tap_skips']=len(re.findall(r'^ok \d+ .*# SKIP',s,re.M))
    if result['tap_passes']-result['tap_skips']<10:failures.append('incomplete upstream ntsync selftests')
if a.test=='oplus':
    m=re.search(r'AVR_OPLUS_SYNC_PASS checks=(\d+)',s)
    result['checks']=int(m[1]) if m else 0
    if result['checks']<1283:failures.append('incomplete sync tests')
result['passed']=not failures
print(json.dumps(result,indent=2))
raise SystemExit(bool(failures))
