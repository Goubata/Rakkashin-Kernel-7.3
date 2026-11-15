#!/usr/bin/env python3
"""Require explicit runtime success; QEMU exit zero alone is insufficient."""
import argparse
import json
import re
from pathlib import Path

p=argparse.ArgumentParser(description=__doc__)
p.add_argument('log',type=Path)
p.add_argument('--test',choices=['geni-map','geni-ring','geni-bus','usb-repeater-pmic','usb-repeater','goodix-mmap','mmi-info-native','mmi-info','mmi-annotate-native','mmi-annotate-driver','moto-touch-of','mmi-annotate','moto-touch-deps','panel-event','power-key','sdam','rtc','pas-driver','pas-images','pas-proxy','mdt-loader','pas-rails','remoteproc-services','smem-state','smp2p','ipcc','glink-transport','rproc-gate','pmic-glink','pmic-ipc','adc-thermal','pmic-gpio','spmi','interconnect','regulators','storage-host','diagnostics','debug-symbol','ufs','ice','storage-phy','smoke','clock','oplus','modules','iommu','secure','sysstats','fence','rakkashin','ntsync','power','adios','platform','scm','gpu-policy','rm-lifetime'],required=True)
p.add_argument('--pas-critical',choices=['auth','shutdown'])
p.add_argument("--clock-count",type=int,default=35)
p.add_argument("--module-count",type=int,default=20)
a=p.parse_args()
if a.pas_critical and a.test!='pas-driver':p.error('--pas-critical requires --test pas-driver')
s=a.log.read_text(errors='replace')
if a.pas_critical:
    mode=a.pas_critical
    message=('Alor PAS image 1 authentication/reset failed: -129' if mode=='auth' else 'Alor PAS cannot confirm firmware shutdown: -5')
    snapshot=('AVR_PAS_CRITICAL_FAILED_AUTH image=1 live=0,1 maps=0,1' if mode=='auth' else 'AVR_PAS_CRITICAL_FAILED_SHUTDOWN live=1,1 maps=1,1')
    failures=[]
    for required in ['AVR_PAS_CRITICAL_ARMED mode='+mode,snapshot,'Kernel panic - not syncing: '+message,'Rebooting in 1 seconds']:
        if required not in s:failures.append('missing '+required)
    for pattern in [r'BUG:',r'WARNING:',r'\bnot ok\b',r'AVR_PAS_CRITICAL_CLEANUP',r'AVR_PAS_CRITICAL_RETURNED',r'AVR_\w*(?:LOAD|UNLOAD)_FAIL']:
        if re.search(pattern,s):failures.append(pattern)
    if s.count('Kernel panic - not syncing:')!=1:failures.append('unexpected panic count')
    meta_path=a.log.with_suffix('.json')
    if not meta_path.is_file():
        failures.append('missing guest termination metadata')
    else:
        meta=json.loads(meta_path.read_text())
        if meta.get('returncode')!=0 or meta.get('pas_critical')!=mode or meta.get('test')!='pas-driver':
            failures.append('guest did not terminate successfully in the selected critical mode')
    print(json.dumps({'log':str(a.log),'test':a.test,'expected_critical_failure':mode,'scope':'expected guest-only kernel panic before cleanup with recorded primitives; no physical firmware operation','passed':not failures,'failures':failures},indent=2))
    raise SystemExit(bool(failures))
markers={'geni-map':'AVR_GENI_MAP_TESTS_PASS scope=native-map-with-recorded-faults-only','geni-ring':'AVR_GENI_RING_TESTS_PASS scope=source-ring-native-dma-private-ram-only','geni-bus':'AVR_GENI_BUS_TESTS_PASS scope=native-wrapper-ram-and-helper-lifecycle-only','usb-repeater-pmic':'AVR_USB_REPEATER_PMIC_TESTS_PASS scope=native-regmap-private-ram-only','usb-repeater':'AVR_USB_REPEATER_TESTS_PASS scope=real-registry-devres-refcounts-only','goodix-mmap':'AVR_GOODIX_MMAP_TESTS_PASS scope=real-vm-selected-source-only','mmi-info-native':'AVR_MMI_INFO_NATIVE_TESTS_PASS scope=production-modules-real-unready-smem-only','mmi-info':'AVR_MMI_INFO_TESTS_PASS scope=actual-info-source-framework-recorded-smem-only','mmi-annotate-native':'AVR_MMI_ANNOTATE_NATIVE_TESTS_PASS scope=production-of-proc-real-minidump-pending-only','mmi-annotate-driver':'AVR_MMI_ANNOTATE_DRIVER_TESTS_PASS scope=real-probe-proc-ram-map-recorded-minidump-only','moto-touch-of':'AVR_MOTO_TOUCH_OF_TESTS_PASS scope=real-of-platform-population-probe-and-relay-api-only','mmi-annotate':'AVR_MMI_ANNOTATE_TESTS_PASS scope=source-append-methods-private-ram-only','moto-touch-deps':'AVR_MOTO_TOUCH_DEPS_TESTS_PASS scope=real-relay-notifier-and-sensor-class-only','panel-event':'AVR_PANEL_EVENT_TESTS_PASS scope=real-notifier-api-and-callback-lifetime-only','power-key':'AVR_POWER_KEY_TESTS_PASS scope=native-irq-input-regmap-ram-only','sdam':'AVR_SDAM_TESTS_PASS scope=factory-callbacks-native-nvmem-regmap-ram-only','rtc':'AVR_RTC_TESTS_PASS scope=native-callbacks-real-regmap-ram-bus-only','pas-driver':'AVR_PAS_DRIVER_TESTS_PASS scope=driver-methods-with-recorded-primitives-only','pas-images':'AVR_PAS_IMAGES_TESTS_PASS scope=recorded-image-ownership-and-registration-only','pas-proxy':'AVR_PAS_PROXY_TESTS_PASS scope=recorded-proxy-ownership-and-registration-only','mdt-loader':'AVR_MDT_LOADER_TESTS_PASS scope=actual-firmware-to-guarded-ram-only','pas-rails':'AVR_PAS_RAILS_TESTS_PASS scope=recorded-regulator-state-machine-and-registration-only','remoteproc-services':'AVR_REMOTEPROC_SERVICES_TESTS_PASS scope=native-callbacks-local-qmi-and-driver-registration-only','smem-state':'AVR_SMEM_STATE_TESTS_PASS scope=native-state-api-and-concurrent-teardown-only','smp2p':'AVR_SMP2P_TESTS_PASS scope=ram-state-and-driver-registration-only','ipcc':'AVR_IPCC_TESTS_PASS scope=ram-registers-and-driver-registration-only','glink-transport':'AVR_GLINK_TRANSPORT_TESTS_PASS scope=ram-ring-wire-and-driver-registration-only','rproc-gate':'AVR_RPROC_GATE_TESTS_PASS scope=missing-or-invalid-transport-error-only','pmic-glink':'AVR_PMIC_GLINK_TESTS_PASS scope=source-callbacks-and-native-local-ipc-only','pmic-ipc':'AVR_PMIC_IPC_TESTS_PASS scope=native-local-ipc-only','adc-thermal':'AVR_ADC_THERMAL_TESTS_PASS scope=conversion-and-module-lifecycle-only','pmic-gpio':'AVR_PMIC_GPIO_TESTS_PASS scope=callback-source-on-ram-only','spmi':'AVR_SPMI_TESTS_PASS scope=ram-regmap-and-native-framework-only','interconnect':'AVR_INTERCONNECT_TESTS_PASS scope=aggregation-and-module-lifecycle-only','regulators':'AVR_REGULATORS_TESTS_PASS scope=logical-rail-and-teardown-only','storage-host':'AVR_STORAGE_HOST_TESTS_PASS scope=module-lifecycle-only','diagnostics':'AVR_DIAGNOSTICS_TESTS_PASS','debug-symbol':'AVR_DEBUG_SYMBOL_TESTS_PASS','ufs':'AVR_UFS_TESTS_PASS','ice':'AVR_ICE_TESTS_PASS','storage-phy':'AVR_STORAGE_PHY_TESTS_PASS count=2','rm-lifetime':'AVR_RM_LIFETIME_TESTS_PASS','gpu-policy':'AVR_GPU_POLICY_TESTS_PASS','scm':'AVR_SCM_TESTS_PASS','platform':'AVR_PLATFORM_TESTS_PASS','smoke':'AVR_QEMU_SMOKE_PASS','clock':'AVR_QEMU_KUNIT_MODULES_LOADED','oplus':'AVR_OPLUS_SYNC_PASS','modules':f'AVR_VENDOR_MODULES_PASS count={a.module_count} ','iommu':'AVR_IOMMU_TESTS_LOADED','secure':'AVR_SECURE_TESTS_LOADED','sysstats':'AVR_SYSSTATS_TESTS_LOADED','fence':'AVR_FENCE_TESTS_LOADED','rakkashin':'AVR_RAKKASHIN_PASS','ntsync':'AVR_NTSYNC_SELFTEST_PASS','power':'AVR_POWER_TESTS_LOADED','adios':'AVR_ADIOS_TESTS_PASS rounds=3 workers=4'}
failures=[]
for pattern in [r'Kernel panic',r'BUG:',r'WARNING:',r'\bnot ok\b',r'AVR_\w*FAIL',r'rcu: .*stall',r'rcu: .*detected stalls']:
    if re.search(pattern,s):failures.append(pattern)
if markers[a.test] not in s:failures.append('missing success marker')
if a.test=='goodix-mmap' and 'AVR_GOODIX_MMAP_USER_PASS assertions=204820 scope=real-vm-selected-source-only' not in s:
    failures.append('missing complete native VM user assertions')
if 'reboot: Power down' not in s:failures.append('guest did not shut down')
result={'log':str(a.log),'test':a.test,'passed':not failures,'failures':failures}
if a.test in ('geni-map','geni-ring','geni-bus','usb-repeater-pmic','usb-repeater','mmi-info-native','mmi-info','mmi-annotate-native','mmi-annotate-driver','moto-touch-of','mmi-annotate','moto-touch-deps','panel-event','power-key','sdam','rtc','pas-driver','pas-images','pas-proxy','mdt-loader','pas-rails','remoteproc-services','smem-state','smp2p','ipcc','glink-transport','rproc-gate','pmic-glink','pmic-ipc','adc-thermal','pmic-gpio','spmi','interconnect','regulators','storage-host','diagnostics','debug-symbol','ufs','ice','clock','iommu','secure','sysstats','fence','power','platform','scm','gpu-policy','rm-lifetime'):
    expected=5 if a.test=='geni-map' else 19 if a.test=='geni-ring' else 7 if a.test=='geni-bus' else 11 if a.test=='usb-repeater-pmic' else 5 if a.test=='usb-repeater' else 2 if a.test=='mmi-info-native' else 13 if a.test=='mmi-info' else 2 if a.test=='mmi-annotate-native' else 11 if a.test=='mmi-annotate-driver' else 1 if a.test=='moto-touch-of' else 8 if a.test=='mmi-annotate' else 9 if a.test=='moto-touch-deps' else 8 if a.test=='panel-event' else 8 if a.test in ('power-key','sdam','rtc') else 13 if a.test=='pas-driver' else 7 if a.test=='pas-images' else 8 if a.test=='pas-proxy' else 7 if a.test=='mdt-loader' else 8 if a.test=='pas-rails' else 11 if a.test=='remoteproc-services' else 3 if a.test=='smem-state' else 8 if a.test=='smp2p' else 7 if a.test=='ipcc' else 11 if a.test=='glink-transport' else 2 if a.test=='rproc-gate' else 19 if a.test=='pmic-glink' else 6 if a.test=='pmic-ipc' else 24 if a.test=='adc-thermal' else 7 if a.test=='pmic-gpio' else 5 if a.test=='spmi' else 9 if a.test=='interconnect' else 7 if a.test=='regulators' else 8 if a.test in ('storage-host','diagnostics') else 5 if a.test=='debug-symbol' else 3 if a.test=='ufs' else 10 if a.test=='ice' else 9 if a.test=='rm-lifetime' else 8 if a.test=='gpu-policy' else 13 if a.test=='scm' else 10 if a.test=='platform' else a.clock_count if a.test=='clock' else 5 if a.test in ('secure','fence','power') else 4 if a.test=='sysstats' else 3
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
if a.test=='mmi-annotate-native' and re.search(r'retaining backing|retaining minidump|minidump removal failed',s):failures.append('unexpected retained dump registration')
result['passed']=not failures
print(json.dumps(result,indent=2))
raise SystemExit(bool(failures))
