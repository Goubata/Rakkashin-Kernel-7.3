#!/usr/bin/env python3
"""Create an isolated RAM-only boot smoke test; it cannot access phone disks."""
from pathlib import Path
from native_module_inventory import firmware_modules
import gzip
import argparse
import os
import stat
import hashlib
import json

ROOT=Path(__file__).resolve().parents[1]
ROOTFS=ROOT.parent/'.andronix/distros/ubuntu/rootfs'

def entry(name,mode,data=b'',rmajor=0,rminor=0,ino=1):
    encoded=name.encode()+b'\0'
    fields=[ino,mode,0,0,1,0,len(data),0,0,rmajor,rminor,len(encoded),0]
    header=b'070701'+b''.join(f'{n:08x}'.encode() for n in fields)
    result=header+encoded
    result+=b'\0'*((-len(result))%4)
    result+=data
    result+=b'\0'*((-len(data))%4)
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    group=parser.add_mutually_exclusive_group()
    group.add_argument("--soc-misc-tests", action="store_true")
    group.add_argument("--media-data-tests", action="store_true")
    group.add_argument("--audio-conn-tests", action="store_true")
    group.add_argument("--msm-drm-tests", action="store_true")
    group.add_argument("--mm-drivers-tests", action="store_true")
    group.add_argument("--intree-misc-tests", action="store_true")
    group.add_argument("--qcom-charging-tests", action="store_true")
    group.add_argument("--qcom-usb-tests", action="store_true")
    group.add_argument("--qcom-pm-tests", action="store_true")
    group.add_argument("--qcom-perf-misc-tests", action="store_true")
    group.add_argument("--qcom-debug-diag-tests", action="store_true")
    group.add_argument("--firststage-misc-tests", action="store_true")
    group.add_argument("--qcom-cpucp-scmi-tests", action="store_true")
    group.add_argument("--qcom-reboot-tests", action="store_true")
    group.add_argument("--qcom-tee-tests", action="store_true")
    group.add_argument("--dma-heaps-tests", action="store_true")
    group.add_argument("--moto-sched-tests", action="store_true")
    group.add_argument("--thermal-pause-tests", action="store_true")
    group.add_argument("--sched-walt-tests", action="store_true")
    group.add_argument("--walt-hooks-tests", action="store_true")
    group.add_argument("--task-vendor-data-tests", action="store_true")
    group.add_argument("--userspace-cdev-tests", action="store_true")
    group.add_argument("--cpu-hotplug-tests", action="store_true")
    group.add_argument("--cpufreq-cooling-tests", action="store_true")
    group.add_argument("--devfreq-cooling-tests", action="store_true")
    group.add_argument("--ddr-cooling-tests", action="store_true")
    group.add_argument("--qmi-cooling-tests", action="store_true")
    group.add_argument("--bcl-pmic-tests", action="store_true")
    group.add_argument("--bcl-soc-tests", action="store_true")
    group.add_argument("--watchdog-tests", action="store_true")
    group.add_argument("--rng-tests", action="store_true")
    group.add_argument("--tsens-tests", action="store_true")
    group.add_argument("--geni-setup-tests", action="store_true")
    group.add_argument("--geni-pm-tests", action="store_true")
    group.add_argument("--geni-map-tests", action="store_true")
    group.add_argument("--geni-ring-tests", action="store_true")
    group.add_argument("--geni-bus-tests", action="store_true")
    group.add_argument("--usb-repeater-pmic-tests", action="store_true")
    group.add_argument("--usb-repeater-tests", action="store_true")
    group.add_argument("--goodix-mmap-tests", action="store_true")
    group.add_argument("--mmi-info-native-tests", action="store_true")
    group.add_argument("--mmi-info-tests", action="store_true")
    group.add_argument("--mmi-annotate-native-tests", action="store_true")
    group.add_argument("--mmi-annotate-driver-tests", action="store_true")
    group.add_argument("--moto-touch-of-tests", action="store_true")
    group.add_argument("--mmi-annotate-tests", action="store_true")
    group.add_argument("--moto-touch-deps-tests", action="store_true")
    group.add_argument("--panel-event-tests", action="store_true")
    group.add_argument("--power-key-tests", action="store_true")
    group.add_argument("--sdam-tests", action="store_true")
    group.add_argument("--rtc-tests", action="store_true")
    group.add_argument("--pas-driver-tests", action="store_true")
    group.add_argument("--pas-images-tests", action="store_true")
    group.add_argument("--pas-proxy-tests", action="store_true")
    group.add_argument("--mdt-loader-tests", action="store_true")
    group.add_argument("--pas-rails-tests", action="store_true")
    group.add_argument("--remoteproc-services-tests", action="store_true")
    group.add_argument("--smem-state-tests", action="store_true")
    group.add_argument("--smp2p-tests", action="store_true")
    group.add_argument("--ipcc-tests", action="store_true")
    group.add_argument("--glink-transport-tests", action="store_true")
    group.add_argument("--rproc-gate-tests", action="store_true")
    group.add_argument("--pmic-glink-tests", action="store_true")
    group.add_argument("--pmic-ipc-tests", action="store_true")
    group.add_argument("--adc-thermal-tests", action="store_true")
    group.add_argument("--pmic-gpio-tests", action="store_true")
    group.add_argument("--spmi-tests", action="store_true")
    group.add_argument("--interconnect-tests", action="store_true")
    group.add_argument("--regulators-tests", action="store_true")
    group.add_argument("--storage-host-tests", action="store_true")
    group.add_argument("--diagnostics-tests", action="store_true")
    group.add_argument("--debug-symbol-tests", action="store_true")
    group.add_argument("--ufs-tests", action="store_true")
    group.add_argument("--ice-tests", action="store_true")
    group.add_argument("--storage-phy-tests", action="store_true")
    group.add_argument("--rm-lifetime-tests", action="store_true")
    group.add_argument("--gpu-policy-tests", action="store_true")
    group.add_argument("--scm-tests", action="store_true")
    group.add_argument("--platform-tests", action="store_true")
    group.add_argument("--adios-tests", action="store_true")
    group.add_argument("--power-tests", action="store_true")
    group.add_argument("--rakkashin-tests", action="store_true")
    group.add_argument("--ntsync-tests", action="store_true")
    group.add_argument("--fence-tests", action="store_true")
    group.add_argument("--sysstats-tests", action="store_true")
    group.add_argument("--secure-tests", action="store_true")
    group.add_argument("--clock-tests", action="store_true")
    group.add_argument("--oplus-tests", action="store_true")
    group.add_argument("--modules-tests", action="store_true")
    group.add_argument("--iommu-tests", action="store_true")
    parser.add_argument("--pas-critical",choices=["auth","shutdown"])
    parser.add_argument("--ufs-mcq",action="store_true")
    args=parser.parse_args()
    if args.pas_critical and not args.pas_driver_tests:parser.error("--pas-critical requires --pas-driver-tests")
    if args.ufs_mcq and not args.ufs_tests:parser.error("--ufs-mcq requires --ufs-tests")
    data=b'';ino=1
    def add(name,mode,body=b'',major=0,minor=0):
        nonlocal data,ino
        data+=entry(name,mode,body,major,minor,ino);ino+=1
    for name in ['bin','dev','proc','sys','tmp','modules']:
        add(name,stat.S_IFDIR|0o755)
    add('dev/console',stat.S_IFCHR|0o600,major=5,minor=1)
    add('dev/null',stat.S_IFCHR|0o666,major=1,minor=3)
    busybox=(ROOTFS/'usr/bin/busybox').read_bytes()
    add('bin/busybox',stat.S_IFREG|0o755,busybox)
    add('bin/sh',stat.S_IFLNK|0o777,b'busybox')
    init=b'''#!/bin/busybox sh
/bin/busybox mount -t proc proc /proc
/bin/busybox mount -t sysfs sysfs /sys
echo AVR_QEMU_SMOKE_BEGIN
/bin/busybox uname -a
echo AVR_QEMU_SMOKE_PASS
/bin/busybox poweroff -f
'''
    if args.clock_tests:
        modules=[(ROOT/'out/native-gki/lib/kunit/kunit.ko','kunit.ko'),
                 (ROOT/'out/native-gki/drivers/clk/clk_kunit_helpers.ko','clk_kunit_helpers.ko')]
        modules += [(ROOT/'tests/clock'/n,n) for n in
                    ['clk-divider_test.ko','clk-gate_test.ko','avr-rate-compat-test.ko','avr-clock-hold-test.ko','avr-rcu-proxy-test.ko']]
        modules += [(ROOT/'out/native-gki/mm/zsmalloc.ko','zsmalloc.ko'),
                    (ROOT/'out/native-gki/drivers/block/zram/zram.ko','zram.ko')]
        modules += [(ROOT/"tests/memory/avr-shmem-pool-test.ko","avr-shmem-pool-test.ko")]
        commands=[]
        for source,name in modules:
            add('modules/'+name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{name}{params} || {{ echo AVR_QEMU_KUNIT_LOAD_FAIL; /bin/busybox poweroff -f; }}')
            if name=='zram.ko':
                commands.append("""echo lz4 > /sys/block/zram0/comp_algorithm &&
echo 67108864 > /sys/block/zram0/disksize &&
devno=$(/bin/busybox cat /sys/block/zram0/dev) &&
/bin/busybox mknod /dev/zram0 b ${devno%:*} ${devno#*:} &&
/bin/busybox mkswap /dev/zram0 &&
/bin/busybox swapon /dev/zram0 || { echo AVR_QEMU_SWAP_FAIL; /bin/busybox poweroff -f; }""")
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS', ('\n'.join(commands)+'\necho AVR_QEMU_KUNIT_MODULES_LOADED').encode())
    if args.oplus_tests:
        add('modules/avr_oplus_sync.ko',stat.S_IFREG|0o644,
            (ROOT/'ports/avr-oplus-sync/avr_oplus_sync.ko').read_bytes())
        add('bin/oplus-sync-test',stat.S_IFREG|0o755,
            (ROOT/'tests/oplus-sync/oplus-sync-test').read_bytes())
        commands = """/bin/busybox insmod /modules/avr_oplus_sync.ko || { echo AVR_OPLUS_LOAD_FAIL; /bin/busybox poweroff -f; }
devno=$(/bin/busybox cat /sys/class/misc/oplus_sync/dev)
/bin/busybox mknod /dev/oplus_sync c ${devno%:*} ${devno#*:}
/bin/oplus-sync-test || { echo AVR_OPLUS_TEST_FAIL; /bin/busybox poweroff -f; }"""
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS',commands.encode())
    if args.modules_tests:
        relative=['qcom-clock-core/crm-v2.ko','qcom-rpmh/cmd-db.ko',
                  'qcom-rpmh/qcom_rpmh.ko']
        modules=[ROOT/'ports'/name for name in relative]
        modules += firmware_modules()
        modules += [ROOT/'ports'/name for name in
                    ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko',
                     'qcom-socinfo/socinfo.ko','qcom-llcc/llcc-qcom.ko',
                     'qcom-dcvs/dcvs_fp.ko','qcom-dcvs/qcom-dcvs.ko',
                     'governor-adreno-ro/governor_msm_adreno_ro.ko',
                     'governor-gpubw/governor_gpubw_mon.ko',
                     'governor-adreno-tz/governor_msm_adreno_tz.ko',
                     'qcom-iommu-util/qcom_iommu_util.ko','secure-buffer/secure_buffer.ko',
                     'iommu-logger/iommu_logger.ko','arm-smmu-vendor/arm_smmu.ko',
                     'msm-sysstats/msm_sysstats.ko']]
        commands=[]
        for source in modules:
            name=source.name
            add('modules/'+name,stat.S_IFREG|0o644,source.read_bytes())
            module=source.stem.replace('-','_')
            commands.append(f'/bin/busybox insmod /modules/{name} || {{ echo AVR_VENDOR_LOAD_FAIL; /bin/busybox poweroff -f; }}')
            commands.append(f'test -d /sys/module/{module} || {{ echo AVR_VENDOR_STATE_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/sysstats-netlink-test',stat.S_IFREG|0o755,
            (ROOT/'tests/sysstats/sysstats-netlink-test').read_bytes())
        commands.append('/bin/sysstats-netlink-test || { echo AVR_SYSSTATS_NETLINK_FAIL; /bin/busybox poweroff -f; }')
        # Core firmware/RPMh providers are intentionally permanent. Preserve
        # their CRM dependency; unload only drivers with real exit callbacks.
        permanent=modules[:6]+[ROOT/'ports/qcom-iommu-util/qcom_iommu_util.ko']
        for source in reversed([m for m in modules if m not in permanent]):
            module=source.stem.replace('-','_')
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_VENDOR_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
            commands.append(f'test ! -d /sys/module/{module} || {{ echo AVR_VENDOR_STATE_FAIL; /bin/busybox poweroff -f; }}')
        for source in permanent:
            module=source.stem.replace('-','_')
            commands.append(f'test -d /sys/module/{module} || {{ echo AVR_VENDOR_STATE_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_VENDOR_MODULES_PASS count=20 unloaded=13 retained=7')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.iommu_tests:
        modules=[(ROOT/'out/native-gki/lib/kunit/kunit.ko',' enable=1')]
        modules += [(p,'') for p in firmware_modules()]
        modules += [(ROOT/'ports/qcom-iommu-util/qcom_iommu_util.ko',''),
                    (ROOT/'tests/iommu/avr-io-pgtable-test.ko','')]
        commands=[]
        for source,params in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_IOMMU_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_IOMMU_TESTS_LOADED')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.secure_tests or args.sysstats_tests or args.fence_tests or args.power_tests:
        fixture='rakkashin-power/avr-wakelock-test.ko' if args.power_tests else 'fence-compat/avr-fence-compat-test.ko' if args.fence_tests else 'sysstats/avr-sysstats-test.ko' if args.sysstats_tests else 'secure-buffer/avr-secure-buffer-test.ko'
        marker='AVR_POWER_TESTS_LOADED' if args.power_tests else 'AVR_FENCE_TESTS_LOADED' if args.fence_tests else 'AVR_SYSSTATS_TESTS_LOADED' if args.sysstats_tests else 'AVR_SECURE_TESTS_LOADED'
        modules=[(ROOT/'out/native-gki/lib/kunit/kunit.ko',' enable=1'),
                 (ROOT/'tests'/fixture,'')]
        commands=[]
        if args.power_tests:
            commands.append('echo avr-test-blocked > /sys/class/misc/boeffla_wakelock_blocker/wakelock_blocker || { echo AVR_POWER_CONFIG_FAIL; /bin/busybox poweroff -f; }')
        for source,params in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SECURE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo '+marker)
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.rakkashin_tests or args.ntsync_tests:
        add('modules/ntsync.ko',stat.S_IFREG|0o644,
            (ROOT/'out/native-gki/drivers/misc/ntsync.ko').read_bytes())
        fixture=ROOT/('tests/rakkashin/rakkashin-test' if args.rakkashin_tests else 'ports/rakkashin/ntsync-selftest')
        name='rakkashin-test' if args.rakkashin_tests else 'ntsync-selftest'
        add('bin/'+name,stat.S_IFREG|0o755,fixture.read_bytes())
        commands=['/bin/busybox insmod /modules/ntsync.ko || { echo AVR_RAKKASHIN_LOAD_FAIL; /bin/busybox poweroff -f; }',
                  'devno=$(/bin/busybox cat /sys/class/misc/ntsync/dev)',
                  '/bin/busybox mknod -m 666 /dev/ntsync c ${devno%:*} ${devno#*:} || { echo AVR_RAKKASHIN_NODE_FAIL; /bin/busybox poweroff -f; }']
        if args.rakkashin_tests:
            commands += ['devno=$(/bin/busybox cat /sys/class/misc/encore_fas/dev)',
                         '/bin/busybox mknod -m 666 /dev/encore_fas c ${devno%:*} ${devno#*:} || { echo AVR_RAKKASHIN_NODE_FAIL; /bin/busybox poweroff -f; }']
        commands.append('/bin/'+name+' || { echo AVR_RAKKASHIN_TEST_FAIL; /bin/busybox poweroff -f; }')
        if args.ntsync_tests:
            commands.append('echo AVR_NTSYNC_SELFTEST_PASS')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.adios_tests:
        for source in [ROOT/'out/native-gki/drivers/block/null_blk/null_blk.ko',
                       ROOT/'ports/rakkashin/adios/adios.ko']:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
        add('bin/adios-io-test',stat.S_IFREG|0o755,(ROOT/'tests/rakkashin/adios-io-test').read_bytes())
        commands=['/bin/busybox insmod /modules/adios.ko || { echo AVR_ADIOS_LOAD_FAIL; /bin/busybox poweroff -f; }',
                  '/bin/busybox insmod /modules/null_blk.ko nr_devices=1 queue_mode=2 memory_backed=1 gb=1 no_sched=0 submit_queues=2 hw_queue_depth=64 || { echo AVR_ADIOS_NULL_FAIL; /bin/busybox poweroff -f; }',
                  'devno=$(/bin/busybox cat /sys/block/nullb0/dev)',
                  '/bin/busybox mknod /dev/nullb0 b ${devno%:*} ${devno#*:} || { echo AVR_ADIOS_NODE_FAIL; /bin/busybox poweroff -f; }']
        for seed,depth in [(1,64),(2,128),(3,64)]:
            commands += ['echo adios > /sys/block/nullb0/queue/scheduler || { echo AVR_ADIOS_SELECT_FAIL; /bin/busybox poweroff -f; }',
                         f'echo {depth} > /sys/block/nullb0/queue/nr_requests || {{ echo AVR_ADIOS_DEPTH_FAIL; /bin/busybox poweroff -f; }}',
                         f'/bin/adios-io-test {seed} || {{ echo AVR_ADIOS_IO_FAIL; /bin/busybox poweroff -f; }}',
                         'echo mq-deadline > /sys/block/nullb0/queue/scheduler || { echo AVR_ADIOS_SWITCH_FAIL; /bin/busybox poweroff -f; }']
        commands += ['/bin/busybox rmmod adios || { echo AVR_ADIOS_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod null_blk || { echo AVR_ADIOS_NULL_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     'echo AVR_ADIOS_TESTS_PASS rounds=3 workers=4']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.mdt_loader_tests:
        for directory in ['lib','lib/firmware','lib/firmware/avr-test']:
            add(directory,stat.S_IFDIR|0o755)
        manifest=json.loads((ROOT/'reports/soccp-loader-stage41-inputs.json').read_text())
        for row in manifest['files']:
            path=ROOT/row['file'];raw=path.read_bytes()
            if hashlib.sha256(raw).hexdigest()!=row['sha256'] or len(raw)!=row['size']:
                raise ValueError('Captured firmware changed: '+str(path))
            add('lib/firmware/avr-test/'+path.name,stat.S_IFREG|0o644,raw)
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+firmware_modules()+[
            ROOT/'ports/qcom-pas-alor/mdt_loader.ko',ROOT/'tests/mdt-loader/avr-mdt-loader-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MDT_LOADER_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_mdt_loader_test','mdt_loader']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_MDT_LOADER_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_MDT_LOADER_TESTS_PASS scope=actual-firmware-to-guarded-ram-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.moto_touch_of_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/moto-touch-deps/mmi_relay.ko',ROOT/'tests/moto-touch-of/avr-moto-touch-of-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MOTO_TOUCH_OF_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_moto_touch_of_test','mmi_relay']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_MOTO_TOUCH_OF_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_MOTO_TOUCH_OF_TESTS_PASS scope=real-of-platform-population-probe-and-relay-api-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.mmi_annotate_native_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','moto-info/mmi_annotate.ko']]+[ROOT/'tests/mmi-annotate-native/avr-mmi-annotate-native-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MMI_ANNOTATE_NATIVE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += [
            'echo proc-interface > /proc/driver/mmi_annotate_mem || { echo AVR_MMI_ANNOTATE_NATIVE_WRITE_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox grep -q proc-interface /proc/driver/mmi_annotate || { echo AVR_MMI_ANNOTATE_NATIVE_READ_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox rmmod avr_mmi_annotate_native_test || { echo AVR_MMI_ANNOTATE_NATIVE_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox rmmod mmi_annotate || { echo AVR_MMI_ANNOTATE_NATIVE_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            '[ ! -e /proc/driver/mmi_annotate ] || { echo AVR_MMI_ANNOTATE_NATIVE_PROC_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox insmod /modules/avr-mmi-annotate-native-test.ko expect_present=0 || { echo AVR_MMI_ANNOTATE_NATIVE_LOAD_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox rmmod avr_mmi_annotate_native_test || { echo AVR_MMI_ANNOTATE_NATIVE_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            'echo AVR_MMI_ANNOTATE_NATIVE_TESTS_PASS scope=production-of-proc-real-minidump-pending-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.geni_setup_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/geni-setup/avr-geni-setup-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_GENI_SETUP_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_geni_setup_test || { echo AVR_GENI_SETUP_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_GENI_SETUP_TESTS_PASS scope=source-setup-native-devres-recorded-channels-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.watchdog_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+firmware_modules()+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','qcom-watchdog/qcom_sdei.ko','qcom-watchdog/qcom_wdt_core.ko','qcom-watchdog/qcom_soc_wdt.ko']]+[ROOT/'tests/watchdog'/name for name in ['avr-watchdog-mmio-test.ko','avr-sdei-test.ko','avr-watchdog-lifetime-test.ko','avr-watchdog-core-test.ko']]
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_WATCHDOG_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_watchdog_core_test','avr_watchdog_lifetime_test','avr_sdei_test','avr_watchdog_mmio_test','qcom_soc_wdt']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_WATCHDOG_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        # Factory watchdog core has no module_exit and retains provider imports;
        # leave it, SDEI and firmware/minidump providers until private guest exit.
        commands.append('echo AVR_WATCHDOG_TESTS_PASS scope=private-ram-recorded-firmware-and-registration-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.bcl_pmic_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','qcom-ipc-logging/qcom_ipc_logging.ko']]+[ROOT/'ports/qcom-bcl-pmic/bcl_pmic5.ko',ROOT/'tests/bcl-pmic/avr-bcl-pmic-test.ko',ROOT/'tests/bcl-pmic/avr-bcl-pmic-probe-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_BCL_PMIC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_bcl_pmic_probe_test || { echo AVR_BCL_PMIC_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('/bin/busybox rmmod avr_bcl_pmic_test || { echo AVR_BCL_PMIC_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('/bin/busybox rmmod bcl_pmic5 || { echo AVR_BCL_PMIC_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_BCL_PMIC_TESTS_PASS scope=native-regmap-thermal-irq-debugfs-private-guest-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.cpufreq_cooling_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qti-cpufreq-cdev/qti_cpufreq_cdev.ko',ROOT/'tests/cpufreq-cooling/avr-cpufreq-cooling-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_CPUFREQ_COOLING_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_cpufreq_cooling_test', 'qti_cpufreq_cdev']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_CPUFREQ_COOLING_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_CPUFREQ_COOLING_TESTS_PASS scope=native-cpufreq-freqqos-cpuhp-thermal-private-driver-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.cpu_hotplug_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qti-cpu-hotplug/cpu_hotplug.ko',ROOT/'tests/cpu-hotplug/avr-cpu-hotplug-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_CPU_HOTPLUG_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_cpu_hotplug_test', 'cpu_hotplug']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_CPU_HOTPLUG_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_CPU_HOTPLUG_TESTS_PASS scope=native-cpuhp-thermal-of-private-sensor-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.userspace_cdev_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qti-userspace-cdev/qti_userspace_cdev.ko',ROOT/'tests/userspace-cdev/avr-userspace-cdev-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_USERSPACE_CDEV_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_userspace_cdev_test', 'qti_userspace_cdev']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_USERSPACE_CDEV_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_USERSPACE_CDEV_TESTS_PASS scope=native-thermal-of-private-sensor-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.task_vendor_data_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/task-vendor-data/avr-task-vendor-data-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            if source.name.startswith('avr-task'):params=' expect_vendor_bytes=$(/bin/busybox sed -n "s/.*avr_expect_vendor=\\([0-9]*\\).*/\\1/p" /proc/cmdline)'
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_TASK_VENDOR_DATA_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_task_vendor_data_test']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_TASK_VENDOR_DATA_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_TASK_VENDOR_DATA_TESTS_PASS scope=native-fork-kthread-dynamic-task-struct')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.walt_hooks_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/walt-hooks/avr-walt-hooks-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_WALT_HOOKS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/binder-pi-test',stat.S_IFREG|0o755,(ROOT/'tests/walt-hooks/binder-pi-test').read_bytes())
        # Restricted hooks are permanent: the probe module stays loaded (as sched_walt does).
        commands += ['/bin/busybox mkdir -p /dev/binderfs',
                     '/bin/busybox mount -t binder binder /dev/binderfs || { echo AVR_WALT_HOOKS_BINDERFS_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/binder-pi-test || { echo AVR_WALT_HOOKS_BINDER_FAIL; /bin/busybox poweroff -f; }',
                     's=$(/bin/busybox cat /sys/module/avr_walt_hooks_test/parameters/binder_set); r=$(/bin/busybox cat /sys/module/avr_walt_hooks_test/parameters/binder_restore); echo "binder hooks set=$s restore=$r"',
                     '[ "$s" -ge 1 ] && [ "$r" -ge 1 ] || { echo AVR_WALT_HOOKS_BINDER_HOOK_FAIL; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_WALT_HOOKS_TESTS_PASS scope=native-sched-binder-hooks-c-binder-binderfs')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.sched_walt_tests:
        sources=[ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/sched-walt/sched-walt.ko',ROOT/'tests/sched-walt/avr-walt-fixture.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            if source.name=='sched-walt.ko':params=' midpoint_freq=2500000 sw_cycle_counter=1'
            if source.name=='avr-walt-fixture.ko' and os.environ.get('AVR_WALT_DIAG')=='nopolicy':params=' per_policy=0'
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SCHED_WALT_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        diag=os.environ.get('AVR_WALT_DIAG','')
        add('bin/walt-check',stat.S_IFREG|0o755,(ROOT/'tests/sched-walt'/({'mixed':'walt-mixed.sh','allwalt':'walt-allwalt.sh'}.get(diag,'walt-check.sh'))).read_bytes())
        # sched_walt is permanent (restricted hooks); the check script validates runtime behaviour.
        commands.append('/bin/walt-check || { echo AVR_SCHED_WALT_CHECK_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_SCHED_WALT_TESTS_PASS scope=real-module-private-cpufreq-em-fixture')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.thermal_pause_tests:
        sources=[ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/sched-walt/sched-walt.ko',ROOT/'ports/qti-thermal-pause/thermal_pause.ko',ROOT/'tests/sched-walt/avr-walt-fixture.ko',ROOT/'tests/thermal-pause/avr-tp-sensor.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            if source.name=='sched-walt.ko':params=' midpoint_freq=2500000 sw_cycle_counter=1'
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_THERMAL_PAUSE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/pause-check',stat.S_IFREG|0o755,(ROOT/'tests/thermal-pause/pause-check.sh').read_bytes())
        commands.append('/bin/pause-check || { echo AVR_THERMAL_PAUSE_CHECK_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_THERMAL_PAUSE_TESTS_PASS scope=real-walt-halt-private-cpufreq-sensor')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.moto_sched_tests:
        sources=[ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/sched-walt/sched-walt.ko',ROOT/'ports/moto-sched/moto_sched.ko',ROOT/'tests/sched-walt/avr-walt-fixture.ko',ROOT/'tests/moto-sched/avr-moto-sched-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            if source.name=='sched-walt.ko':params=' midpoint_freq=2500000 sw_cycle_counter=1'
            if source.name=='avr-moto-sched-test.ko':
                # walt must be initialised and moto locking enabled (BASE|LOCK|BINDER|PERCPU_RWSEM)
                commands.append('i=0; while [ ! -d /proc/sys/walt ] && [ $i -lt 100 ]; do /bin/busybox usleep 100000; i=$((i+1)); done')
                commands.append('echo 100d > /proc/moto_sched/enabled || { echo AVR_MOTO_SCHED_ENABLE_FAIL; /bin/busybox poweroff -f; }')
                add('modules/kunit.ko',stat.S_IFREG|0o644,(ROOT/'out/native-gki/lib/kunit/kunit.ko').read_bytes())
                commands.append('/bin/busybox insmod /modules/kunit.ko enable=1 || { echo AVR_MOTO_SCHED_LOAD_FAIL; /bin/busybox poweroff -f; }')
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MOTO_SCHED_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/vma-name-test',stat.S_IFREG|0o755,(ROOT/'tests/moto-sched/vma-name-test').read_bytes())
        add('bin/binder-pi-test',stat.S_IFREG|0o755,(ROOT/'tests/walt-hooks/binder-pi-test').read_bytes())
        commands += ['/bin/vma-name-test || { echo AVR_MOTO_SCHED_VMA_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox mkdir -p /dev/binderfs; /bin/busybox mount -t binder binder /dev/binderfs',
                     '/bin/binder-pi-test || { echo AVR_MOTO_SCHED_BINDER_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\|BUG:\\|Oops\\|possible circular" && { echo AVR_MOTO_SCHED_KERNEL_WARN; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_MOTO_SCHED_TESTS_PASS scope=real-walt-moto-sched-proxy-exec')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.dma_heaps_tests:
        sources=[ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'out/native-gki/drivers/firmware/qcom/qcom-scm.ko',ROOT/'ports/secure-buffer/secure_buffer.ko',ROOT/'ports/gunyah/gh_arm_drv.ko',ROOT/'ports/gunyah/gh_dbl.ko',ROOT/'ports/gunyah/gh_msgq.ko',ROOT/'tests/dma-heaps/avr-gunyah-stub.ko',ROOT/'ports/gunyah-rm/gh_rm_drv.ko',ROOT/'ports/qcom-dma-heaps/mem-prot.ko',ROOT/'ports/qcom-dma-heaps/gh_mem_notifier.ko',ROOT/'ports/qcom-dma-heaps/mem_buf_dev.ko',ROOT/'ports/qcom-dma-heaps/msm_dma_iommu_mapping.ko',ROOT/'ports/qcom-dma-heaps/deferred-free-helper.ko',ROOT/'ports/qcom-dma-heaps/qcom_dma_heaps.ko',ROOT/'ports/qcom-dma-heaps/mem_buf_msgq.ko',ROOT/'ports/qcom-dma-heaps/mem_buf.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_DMA_HEAPS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/dma-heap-test',stat.S_IFREG|0o755,(ROOT/'tests/dma-heaps/dma-heap-test').read_bytes())
        commands += ['/bin/busybox mkdir -p /dev/dma_heap; for h in /sys/class/dma_heap/*; do n=${h##*/}; /bin/busybox mknod "/dev/dma_heap/$n" c $(/bin/busybox sed "s/:/ /" $h/dev); done',
                     '/bin/busybox ls /dev/dma_heap/ /sys/kernel/dma_heap/',
                     '/bin/dma-heap-test || { echo AVR_DMA_HEAPS_USER_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }',
                     '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\|BUG:\\|Oops\\|possible circular" && { echo AVR_DMA_HEAPS_KERNEL_WARN; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_DMA_HEAPS_TESTS_PASS scope=real-qcom-heaps-userspace-alloc')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_tee_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/secure-buffer/secure_buffer.ko',ROOT/'ports/gunyah/gh_arm_drv.ko',ROOT/'ports/gunyah/gh_dbl.ko',ROOT/'ports/gunyah/gh_msgq.ko',ROOT/'tests/dma-heaps/avr-gunyah-stub.ko',ROOT/'ports/gunyah-rm/gh_rm_drv.ko',ROOT/'ports/qcom-dma-heaps/mem-prot.ko',ROOT/'ports/qcom-dma-heaps/gh_mem_notifier.ko',ROOT/'ports/qcom-dma-heaps/mem_buf_dev.ko',ROOT/'ports/qcom-dma-heaps/msm_dma_iommu_mapping.ko',ROOT/'ports/qcom-dma-heaps/deferred-free-helper.ko',ROOT/'ports/qcom-dma-heaps/qcom_dma_heaps.ko',ROOT/'ports/qcom-dma-heaps/mem_buf_msgq.ko',ROOT/'ports/qcom-dma-heaps/mem_buf.ko',ROOT/'ports/qcom-tee/qseecom_proxy.ko',ROOT/'ports/qcom-tee/si_core_module.ko',ROOT/'ports/securemsm/smcinvoke_dlkm.ko',ROOT/'tests/qcom-tee/avr-tee-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_TEE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/smcinvoke-test',stat.S_IFREG|0o755,(ROOT/'tests/qcom-tee/smcinvoke-test').read_bytes())
        commands += ['for d in /sys/class/smcinvoke/*; do /bin/busybox mknod "/dev/${d##*/}" c $(/bin/busybox sed "s/:/ /" $d/dev); done',
                     '/bin/busybox ls -l /dev/smcinvoke',
                     '/bin/smcinvoke-test || { echo AVR_QCOM_TEE_USER_FAIL; /bin/busybox dmesg | /bin/busybox tail -60; /bin/busybox poweroff -f; }',
                     '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\|BUG:\\|Oops\\|possible circular\\|Invalid wait" && { echo AVR_QCOM_TEE_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A20 "WARNING:\\|BUG:\\|Invalid wait"; /bin/busybox poweroff -f; }']
        for module in ['avr_tee_test']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_QCOM_TEE_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_QCOM_TEE_TESTS_PASS scope=shmbridge-registry-si-core-fake-tz')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_reboot_tests:
        sources=[ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'tests/qcom-reboot/avr-minidump-stub.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/qcom-reboot/qcom-dload-mode.ko',ROOT/'ports/qcom-reboot/qcom-reboot-reason.ko',ROOT/'ports/qcom-reboot/moto_reboot_reason.ko',ROOT/'ports/qcom-reboot/pmic-pon-log.ko',ROOT/'tests/qcom-reboot/avr-fake-sdam.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' name=mini' if source.name=='qcom-dload-mode.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_REBOOT_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/reboot2-test',stat.S_IFREG|0o755,(ROOT/'tests/qcom-reboot/reboot2-test').read_bytes())
        commands += ['/bin/busybox cat /sys/kernel/dload/dload_mode /sys/module/qcom_scm/parameters/avr_state', '/bin/busybox grep -q "mini" /sys/kernel/dload/dload_mode || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox grep -q "io=0x1fd9000:0x20 " /sys/module/qcom_scm/parameters/avr_state || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', 'echo full > /sys/kernel/dload/dload_mode && /bin/busybox grep -q ":0x10 " /sys/module/qcom_scm/parameters/avr_state || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', 'echo both > /sys/kernel/dload/dload_mode && /bin/busybox grep -q ":0x30 " /sys/module/qcom_scm/parameters/avr_state && /bin/busybox grep -q both /sys/kernel/dload/dload_mode || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', 'echo bogus > /sys/kernel/dload/dload_mode 2>/dev/null && { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', 'S0=$(/bin/busybox sed "s/.*sdi=\\([0-9]*\\).*/\\1/" /sys/module/qcom_scm/parameters/avr_state); echo 0 > /sys/module/qcom_dload_mode/parameters/download_mode; /bin/busybox grep -q ":0x0 " /sys/module/qcom_scm/parameters/avr_state || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }; S1=$(/bin/busybox sed "s/.*sdi=\\([0-9]*\\).*/\\1/" /sys/module/qcom_scm/parameters/avr_state); [ "$S1" -gt "$S0" ] || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', 'echo 1 > /sys/module/qcom_dload_mode/parameters/download_mode; /bin/busybox grep -q ":0x30 " /sys/module/qcom_scm/parameters/avr_state || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox cat /sys/module/qcom_scm/parameters/avr_state', '/bin/busybox cat /proc/driver/mot_pon_log', '/bin/busybox grep -qi "PON" /proc/driver/mot_pon_log || { echo AVR_QCOM_REBOOT_STEP_FAIL; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops\\\\|Invalid wait" && { echo AVR_QCOM_REBOOT_KERNEL_WARN; /bin/busybox poweroff -f; }', 'echo 2 > /sys/module/avr_fake_sdam/parameters/expect_r48; echo 4 > /sys/module/avr_fake_sdam/parameters/expect_r49; echo 0 > /sys/module/avr_fake_sdam/parameters/expect_dload; echo 1 > /sys/module/avr_fake_sdam/parameters/snapshot_sdi', '/bin/reboot2-test bootloader', 'echo AVR_QCOM_REBOOT_RETURNED; /bin/busybox poweroff -f']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_cpucp_scmi_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-cpucp-scmi/qcom_scmi_vendor.ko',ROOT/'ports/qcom-cpucp-scmi/qcom_scmi_client.ko',ROOT/'ports/qcom-cpucp-scmi/qcom_cpucp.ko',ROOT/'ports/qcom-cpucp-scmi/cpu_phys_log_map.ko',ROOT/'ports/qcom-cpucp-scmi/cpucp_log.ko',ROOT/'ports/qcom-cpucp-scmi/qcom-cpufreq-hw.ko',ROOT/'out/native-gki/drivers/clk/scmi-clk.ko',ROOT/'tests/qcom-cpucp-scmi/avr-fake-scmi.ko',ROOT/'tests/qcom-cpucp-scmi/avr-scmi-vendor-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_CPUCP_SCMI_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['for p in 0 2; do /bin/busybox head /sys/devices/system/cpu/cpufreq/policy$p/scaling_driver /sys/devices/system/cpu/cpufreq/policy$p/related_cpus /sys/devices/system/cpu/cpufreq/policy$p/scaling_available_frequencies /sys/devices/system/cpu/cpufreq/policy$p/scaling_boost_frequencies /sys/devices/system/cpu/cpufreq/policy$p/cpuinfo_transition_latency; done', '/bin/busybox grep . /sys/devices/system/cpu/cpufreq/boost /sys/devices/system/cpu/cpufreq/policy*/boost /sys/devices/system/cpu/cpufreq/policy*/cpuinfo_max_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_max_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/module/avr_fake_scmi/parameters/state', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/scaling_driver)" = scmi ] || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/related_cpus)" = "0 1" ] && [ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy2/related_cpus)" = "2 3" ] || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_frequencies)" = "384000 556800 710400 806400 902400 1036800 1152000 1267200 1401600 1555200 1651200 1747200 1900800 2016000 2188800 2265600 2361600 2476800 2611200 2707200 2841600 3033600 3129600 3321600 " ] || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy2/scaling_available_frequencies)" = "384000 556800 806400 921600 1017600 1152000 1248000 1382400 1497600 1651200 1862400 1996800 2112000 2227200 2476800 2688000 2841600 2995200 3091200 3244800 3436800 " ] || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy2/scaling_boost_frequencies)" = "3532800 3648000 3724800 3801600 " ] || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/cpuinfo_transition_latency)" = 30000 ] || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', 'echo performance > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor && echo 1555200 > /sys/devices/system/cpu/cpufreq/policy0/scaling_max_freq && /bin/busybox sleep 1 && /bin/busybox grep -q "level=1555200," /sys/module/avr_fake_scmi/parameters/state || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/scaling_cur_freq)" = 1555200 ] || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', 'echo performance > /sys/devices/system/cpu/cpufreq/policy2/scaling_governor && /bin/busybox sleep 1 && [ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/boost)" = 1 ] && /bin/busybox grep -q ",3801600 " /sys/module/avr_fake_scmi/parameters/state || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', 'echo 0 > /sys/devices/system/cpu/cpufreq/boost && /bin/busybox sleep 1 && /bin/busybox grep -q ",3436800 " /sys/module/avr_fake_scmi/parameters/state || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', 'echo 1 > /sys/devices/system/cpu/cpufreq/boost && /bin/busybox sleep 1 && [ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy2/scaling_max_freq)" = 3801600 ] && /bin/busybox grep -q ",3801600 " /sys/module/avr_fake_scmi/parameters/state || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', 'echo 2000000 > /sys/devices/system/cpu/cpufreq/policy2/scaling_max_freq && /bin/busybox sleep 1 && /bin/busybox grep -q ",1996800 " /sys/module/avr_fake_scmi/parameters/state || { echo AVR_QCOM_CPUCP_SCMI_STEP_FAIL; /bin/busybox cat /sys/module/avr_fake_scmi/parameters/state /sys/devices/system/cpu/cpufreq/policy*/scaling_cur_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '/bin/busybox cat /sys/module/avr_fake_scmi/parameters/state', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops\\\\|Invalid wait" && { echo AVR_QCOM_CPUCP_SCMI_KERNEL_WARN; /bin/busybox poweroff -f; }']
        for module in ['avr_scmi_vendor_test']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_QCOM_CPUCP_SCMI_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_QCOM_CPUCP_SCMI_TESTS_PASS scope=scmi-perf-cpufreq-vendor-fake-cpucp')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.firststage_misc_tests:
        sources=[ROOT/'ports/qcom-firststage-misc/icc-clk.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-remoteproc-services/qcom_aoss.ko',ROOT/'ports/qcom-firststage-misc/msm_qmp.ko',ROOT/'ports/qcom-firststage-misc/qcom_smd.ko',ROOT/'ports/qcom-firststage-misc/sg.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_FIRSTSTAGE_MISC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/sg-test',stat.S_IFREG|0o755,(ROOT/'tests/firststage-misc/sg-test').read_bytes())
        commands += ['tries=0; while test ! -e /sys/block/sda/device/scsi_generic; do tries=$((tries+1)); if test "$tries" -gt 100; then echo AVR_FIRSTSTAGE_MISC_ENUM_FAIL; /bin/busybox poweroff -f; fi; /bin/busybox sleep 0.1; done', 'sg=$(/bin/busybox ls /sys/block/sda/device/scsi_generic); devno=$(/bin/busybox cat /sys/class/scsi_generic/$sg/dev); /bin/busybox mknod /dev/$sg c ${devno%:*} ${devno#*:}; echo sg=$sg', '/bin/sg-test /dev/$sg /sys/block/sda/device/power/runtime_usage || { echo AVR_FIRSTSTAGE_MISC_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox ls /sys/class/scsi_generic/', '/bin/busybox grep -E "^(sg|icc_clk|qcom_smd|msm_qmp) " /proc/modules', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_FIRSTSTAGE_MISC_KERNEL_WARN; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_FIRSTSTAGE_MISC_TESTS_PASS scope=sg-ufs-autopm-icc-smd-qmp-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_debug_diag_tests:
        sources=[ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/qcom-iommu-util/qcom_iommu_util.ko',ROOT/'ports/qcom-debug-diag/kryo_arm64_edac.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-debug-diag/memory_dump_v2.ko',ROOT/'ports/qcom-debug-diag/qcom_iommu_debug.ko',ROOT/'ports/qcom-debug-diag/qcom_logbuf_boot_log.ko',ROOT/'ports/qcom-debug-diag/qcom_logbuf_vendor_hooks.ko',ROOT/'ports/qcom-debug-diag/qcom_va_minidump.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_DEBUG_DIAG_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox cat /sys/module/qcom_scm/parameters/avr_state', '/bin/busybox grep -q "dump=1:" /sys/module/qcom_scm/parameters/avr_state || { echo AVR_QCOM_DEBUG_DIAG_STEP_FAIL; /bin/busybox dmesg | /bin/busybox grep -i "dump\\\\|minidump\\\\|edac\\\\|logbuf" | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox grep -q "table=200000/1 apps=200000/5$" /sys/module/qcom_scm/parameters/avr_state || { echo AVR_QCOM_DEBUG_DIAG_STEP_FAIL; /bin/busybox dmesg | /bin/busybox grep -i "dump\\\\|minidump\\\\|edac\\\\|logbuf" | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox grep -E "^(memory_dump_v2|kryo_arm64_edac|qcom_iommu_debug|qcom_logbuf_boot_log|qcom_logbuf_vendor_hooks|qcom_va_minidump) " /proc/modules | /bin/busybox wc -l | /bin/busybox grep -q "^6$" || { echo AVR_QCOM_DEBUG_DIAG_STEP_FAIL; /bin/busybox dmesg | /bin/busybox grep -i "dump\\\\|minidump\\\\|edac\\\\|logbuf" | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -i "memory dump\\\\|mem_dump\\\\|MSM Memory Dump" | /bin/busybox head -5', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_QCOM_DEBUG_DIAG_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A15 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_QCOM_DEBUG_DIAG_TESTS_PASS scope=memdump-table-tz-imem-diag-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_perf_misc_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-cpucp-scmi/qcom_scmi_vendor.ko',ROOT/'ports/qcom-cpucp-scmi/qcom_scmi_client.ko',ROOT/'ports/qcom-cpucp-scmi/cpu_phys_log_map.ko',ROOT/'ports/qcom-perf-misc/qcom_llcc_pmu.ko',ROOT/'ports/qcom-perf-misc/qcom-pmu-lib.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/sched-walt/sched-walt.ko',ROOT/'ports/qcom-perf-misc/msm_performance.ko',ROOT/'ports/qcom-perf-misc/qcom_cpu_vendor_hooks.ko',ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/qcom-perf-misc/arm_smmu_pmu.ko',ROOT/'tests/qcom-cpucp-scmi/avr-fake-scmi.ko',ROOT/'tests/qcom-perf-misc/avr-perf-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ' midpoint_freq=2500000 sw_cycle_counter=1' if source.name=='sched-walt.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_PERF_MISC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox ls /sys/bus/platform/drivers/ | /bin/busybox grep -i "qcom\|pmu\|hook"; /bin/busybox ls /sys/bus/platform/drivers/qcom-cpu-vendor-hooks/ /sys/bus/platform/drivers/qcom-pmu/; /bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors', '/bin/busybox ls /sys/bus/platform/drivers/qcom-cpu-vendor-hooks/ | /bin/busybox grep -q cpu-vendor-hooks || { echo AVR_QCOM_PERF_MISC_STEP_FAIL; /bin/busybox grep . /sys/devices/system/cpu/cpufreq/policy*/scaling_*_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -25; /bin/busybox poweroff -f; }', '/bin/busybox ls /sys/bus/platform/drivers/qcom-pmu/ | /bin/busybox grep -q pmu || { echo AVR_QCOM_PERF_MISC_STEP_FAIL; /bin/busybox grep . /sys/devices/system/cpu/cpufreq/policy*/scaling_*_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -25; /bin/busybox poweroff -f; }', 'i=0; while ! /bin/busybox grep -q walt /sys/devices/system/cpu/cpufreq/policy0/scaling_available_governors && [ $i -lt 150 ]; do /bin/busybox usleep 100000; i=$((i+1)); done; echo walt_wait=$i; /bin/busybox ls /proc/sys/walt | /bin/busybox wc -l', 'echo walt > /sys/devices/system/cpu/cpufreq/policy0/scaling_governor && echo walt > /sys/devices/system/cpu/cpufreq/policy2/scaling_governor || { echo AVR_QCOM_PERF_MISC_STEP_FAIL; /bin/busybox grep . /sys/devices/system/cpu/cpufreq/policy*/scaling_*_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -25; /bin/busybox poweroff -f; }', 'echo "2:1996800" > /sys/kernel/msm_performance/parameters/cpu_max_freq && /bin/busybox sleep 1 && [ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy2/scaling_max_freq)" = 1996800 ] || { echo AVR_QCOM_PERF_MISC_STEP_FAIL; /bin/busybox grep . /sys/devices/system/cpu/cpufreq/policy*/scaling_*_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -25; /bin/busybox poweroff -f; }', 'echo "0:1555200" > /sys/kernel/msm_performance/parameters/cpu_min_freq && /bin/busybox sleep 1 && [ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq)" = 1555200 ] || { echo AVR_QCOM_PERF_MISC_STEP_FAIL; /bin/busybox grep . /sys/devices/system/cpu/cpufreq/policy*/scaling_*_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -25; /bin/busybox poweroff -f; }', '/bin/busybox cat /sys/kernel/msm_performance/parameters/cpu_max_freq /sys/kernel/msm_performance/parameters/cpu_min_freq', 'echo "2:4294967295" > /sys/kernel/msm_performance/parameters/cpu_max_freq && echo "0:0" > /sys/kernel/msm_performance/parameters/cpu_min_freq && /bin/busybox sleep 1 && [ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy2/scaling_max_freq)" = 3801600 ] && [ "$(/bin/busybox cat /sys/devices/system/cpu/cpufreq/policy0/scaling_min_freq)" = 384000 ] || { echo AVR_QCOM_PERF_MISC_STEP_FAIL; /bin/busybox grep . /sys/devices/system/cpu/cpufreq/policy*/scaling_*_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -25; /bin/busybox poweroff -f; }', '/bin/busybox grep -E "^(arm_smmu_pmu|qcom_llcc_pmu) " /proc/modules | /bin/busybox wc -l | /bin/busybox grep -q "^2$" || { echo AVR_QCOM_PERF_MISC_STEP_FAIL; /bin/busybox grep . /sys/devices/system/cpu/cpufreq/policy*/scaling_*_freq /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; /bin/busybox dmesg | /bin/busybox tail -25; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops\\\\|Invalid wait" && { echo AVR_QCOM_PERF_MISC_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A20 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }']
        for module in ['avr_perf_test']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_QCOM_PERF_MISC_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_QCOM_PERF_MISC_TESTS_PASS scope=pmu-lib-msm-perf-walt-scmi-hooks')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_pm_tests:
        sources=[ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/sched-walt/sched-walt.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'ports/qcom-pm/qcom_lpm.ko',ROOT/'ports/qcom-pm/qcom_lpm_monitor.ko',ROOT/'ports/qcom-pm/msm_show_resume_irq.ko',ROOT/'ports/qcom-pm/msm_show_epoch.ko',ROOT/'ports/qcom-remoteproc-services/qcom_aoss.ko',ROOT/'ports/qcom-pm/qcom_stats.ko',ROOT/'ports/qcom-pm/sys_pm_vx.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/qcom-pm/qcom_cpuss_sleep_stats.ko',ROOT/'ports/qcom-pm-telemetry/cpuss_telemetry.ko',ROOT/'ports/qcom-pm/qcom_cpuss_sleep_stats_v4.ko',ROOT/'ports/qcom-pm/smp2p_sleepstate.ko',ROOT/'ports/qcom-pm/boot_stats.ko',ROOT/'ports/qcom-pm/sysmon_subsystem_stats.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' midpoint_freq=2500000 sw_cycle_counter=1' if source.name=='sched-walt.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_PM_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox cat /sys/devices/system/cpu/cpuidle/current_driver /sys/devices/system/cpu/cpuidle/current_governor /sys/devices/system/cpu/cpuidle/available_governors', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpuidle/current_governor)" = qcom-cpu-lpm ] || echo qcom-cpu-lpm > /sys/devices/system/cpu/cpuidle/current_governor', '[ "$(/bin/busybox cat /sys/devices/system/cpu/cpuidle/current_governor)" = qcom-cpu-lpm ] || { echo AVR_QCOM_PM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox cat /sys/power/mem_sleep; echo 1 > /sys/module/msm_show_resume_irq/parameters/debug_mask', 'for m in s2idle deep; do /bin/busybox grep -q "$m" /sys/power/mem_sleep || { echo skip_$m; continue; }; echo $m > /sys/power/mem_sleep; echo 0 > /sys/class/rtc/rtc0/wakealarm; echo +2 > /sys/class/rtc/rtc0/wakealarm; echo mem > /sys/power/state || { echo AVR_QCOM_PM_SUSPEND_FAIL_$m; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }; echo resumed_$m; done', '/bin/busybox cat /sys/power/suspend_stats/success /sys/power/suspend_stats/fail', '/bin/busybox dmesg | /bin/busybox grep -iE "PM: suspend (entry|exit)|resume_irq|HWIRQ|epoch|Re-initializing gic" | /bin/busybox tail -12', '[ "$(/bin/busybox cat /sys/power/suspend_stats/fail)" = 0 ] && [ "$(/bin/busybox cat /sys/power/suspend_stats/success)" -ge 1 ] || { echo AVR_QCOM_PM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -30; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops\\\\|Invalid wait" && { echo AVR_QCOM_PM_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A20 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_QCOM_PM_TESTS_PASS scope=lpm-governor-s2idle-wake-pm-modules')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_usb_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-usb/redriver.ko',ROOT/'ports/qcom-clock-core/proxy-consumer.ko',ROOT/'ports/qcom-regulators/debug-regulator.ko',ROOT/'ports/qcom-clock-core/gdsc-regulator.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-clock-core/crm-v2.ko',ROOT/'ports/qcom-firststage-misc/icc-clk.ko',ROOT/'ports/qcom-clock-core/clk-qcom.ko',ROOT/'ports/usb-repeater/repeater.ko',ROOT/'ports/qcom-spmi/qti-regmap-debugfs.ko',ROOT/'ports/qcom-display-deps/wcd_usbss_i2c.ko',ROOT/'ports/qcom-usb/dwc3-msm.ko',ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/qcom-usb/eud.ko',ROOT/'ports/qcom-usb/nb7vpq904m.ko',ROOT/'ports/qcom-usb/phy-msm-snps-eusb2.ko',ROOT/'ports/qcom-usb/phy-msm-snps-hs.ko',ROOT/'ports/qcom-eom/eom_driver.ko',ROOT/'ports/qcom-usb/phy-msm-ssusb-qmp.ko',ROOT/'ports/qcom-usb/phy-qcom-emu.ko',ROOT/'ports/qcom-usb/phy-qcom-eusb2-repeater.ko',ROOT/'ports/qcom-usb/ps5169.ko',ROOT/'tests/qcom-usb/avr-usb-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_USB_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_usb_test']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_QCOM_USB_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_QCOM_USB_TESTS_PASS scope=dwc3-msm-kretprobes-usb-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qcom_charging_tests:
        sources=[ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-glink-transport/qcom_glink.ko',ROOT/'ports/qcom-glink-transport/qcom_glink_smem.ko',ROOT/'ports/qcom-rproc-common/qcom_common.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'out/native-gki/drivers/soc/qcom/qcom_pdr_msg.ko',ROOT/'out/native-gki/drivers/soc/qcom/pdr_interface.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-pmic-glink/qti_pmic_glink.ko',ROOT/'ports/qcom-charging/altmode-glink.ko',ROOT/'ports/qcom-charging/charger-ulog-glink.ko',ROOT/'ports/qcom-charging/glink_pkt.ko',ROOT/'ports/qcom-charging/pmic-glink-debug.ko',ROOT/'ports/qcom-charging/qcom_pd_mapper.ko',ROOT/'ports/qcom-charging/qti-glink-adc.ko',ROOT/'ports/qcom-panel-event/panel_event_notifier.ko',ROOT/'ports/qcom-charging/qti_battery_charger.ko',ROOT/'ports/qcom-charging/qti_battery_debug.ko',ROOT/'ports/qcom-charging/ucsi_qti_glink.ko',ROOT/'ports/moto-charging/bm_adsp_ulog.ko',ROOT/'ports/moto-touch-deps/sensors_class.ko',ROOT/'ports/moto-charging/hall_phone_case.ko',ROOT/'ports/moto-info/mmi_annotate.ko',ROOT/'ports/moto-info/mmi_info.ko',ROOT/'ports/moto-charging/qti_glink_charger_v2.ko',ROOT/'ports/qcom-reboot/moto_reboot_reason.ko',ROOT/'ports/moto-charging/mmi_glink_charger.ko',ROOT/'ports/moto-charging/qpnp_adaptive_charge.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QCOM_CHARGING_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox grep -cE "^(qti_battery_charger|altmode_glink|ucsi_qti_glink|charger_ulog_glink|qti_battery_debug|qti_glink_adc|pmic_glink_debug|glink_pkt|qcom_pd_mapper|mmi_glink_charger|qti_glink_charger_v2|qpnp_adaptive_charge|bm_adsp_ulog|hall_phone_case) " /proc/modules', '[ "$(/bin/busybox grep -cE "^(qti_battery_charger|altmode_glink|ucsi_qti_glink|charger_ulog_glink|qti_battery_debug|qti_glink_adc|pmic_glink_debug|glink_pkt|qcom_pd_mapper|mmi_glink_charger|qti_glink_charger_v2|qpnp_adaptive_charge|bm_adsp_ulog|hall_phone_case) " /proc/modules)" = 14 ] || { echo AVR_QCOM_CHARGING_STEP_FAIL; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_QCOM_CHARGING_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A20 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_QCOM_CHARGING_TESTS_PASS scope=charging-glink-clients-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.intree_misc_tests:
        sources=[ROOT/'out/native-gki/net/tls/tls.ko',ROOT/'out/native-gki/drivers/hwmon/hwmon.ko',ROOT/'out/native-gki/drivers/usb/phy/phy-generic.ko',ROOT/'out/native-gki/drivers/leds/trigger/ledtrig-pattern.ko',ROOT/'out/native-gki/drivers/i2c/busses/i2c-qcom-geni.ko',ROOT/'out/native-gki/drivers/spi/spi-geni-qcom.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_INTREE_MISC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox ls /sys/bus/platform/drivers/usb_phy_generic/', '/bin/busybox ls /sys/bus/platform/drivers/usb_phy_generic/ | /bin/busybox grep -q usb_nop_phy || { echo AVR_INTREE_MISC_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }', '[ "$(/bin/busybox grep -cE "^(tls|hwmon|phy_generic|ledtrig_pattern|i2c_qcom_geni|spi_geni_qcom) " /proc/modules)" = 6 ] || { echo AVR_INTREE_MISC_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -20; /bin/busybox poweroff -f; }']
        commands.append('echo AVR_INTREE_MISC_TESTS_PASS scope=intree-gki-equivalent-modules-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.mm_drivers_tests:
        sources=[ROOT/'ports/qcom-clock-core/proxy-consumer.ko',ROOT/'ports/qcom-regulators/debug-regulator.ko',ROOT/'ports/qcom-clock-core/gdsc-regulator.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-clock-core/crm-v2.ko',ROOT/'ports/qcom-firststage-misc/icc-clk.ko',ROOT/'ports/qcom-clock-core/clk-qcom.ko',ROOT/'ports/mm-drivers/msm-mmrm.ko',ROOT/'ports/mm-drivers/msm_ext_display.ko',ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'ports/qcom-pas-alor/mdt_loader.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/mm-drivers/msm_hfi_core.ko',ROOT/'ports/mm-drivers/sync_fence.ko',ROOT/'ports/qcom-sps/sps_drv.ko',ROOT/'ports/qcom-tee/qseecom_proxy.ko',ROOT/'ports/secure-buffer/secure_buffer.ko',ROOT/'ports/gunyah/gh_arm_drv.ko',ROOT/'ports/gunyah/gh_dbl.ko',ROOT/'ports/gunyah/gh_msgq.ko',ROOT/'tests/dma-heaps/avr-gunyah-stub.ko',ROOT/'ports/gunyah-rm/gh_rm_drv.ko',ROOT/'ports/qcom-dma-heaps/mem-prot.ko',ROOT/'ports/qcom-dma-heaps/gh_mem_notifier.ko',ROOT/'ports/qcom-dma-heaps/mem_buf_dev.ko',ROOT/'ports/qcom-tee/si_core_module.ko',ROOT/'ports/securemsm/smcinvoke_dlkm.ko',ROOT/'ports/securemsm/hdcp_qseecom_dlkm.ko',ROOT/'ports/securemsm/qce50_dlkm.ko',ROOT/'ports/securemsm/qcedev-mod_dlkm.ko',ROOT/'ports/securemsm/qcrypto-msm_dlkm.ko',ROOT/'out/native-gki/crypto/rng.ko',ROOT/'ports/securemsm/qrng_dlkm.ko',ROOT/'ports/qcom-dma-heaps/msm_dma_iommu_mapping.ko',ROOT/'ports/qcom-dma-heaps/deferred-free-helper.ko',ROOT/'ports/qcom-dma-heaps/qcom_dma_heaps.ko',ROOT/'ports/securemsm/smmu_proxy_dlkm.ko',ROOT/'ports/securemsm/tmecom-intf_dlkm.ko',ROOT/'ports/securemsm/tz_log_dlkm.ko',ROOT/'out/native-gki/lib/crypto/libsha3.ko',ROOT/'out/native-gki/crypto/jitterentropy_rng.ko',ROOT/'out/native-gki/crypto/drbg.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MM_DRIVERS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/spec-sync-test',stat.S_IFREG|0o755,(ROOT/'tests/mm-drivers/spec-sync-test').read_bytes())
        commands += ['/bin/busybox mount -t debugfs none /sys/kernel/debug 2>/dev/null; true', 'devno=$(/bin/busybox cat /sys/class/sync/spec_sync/dev) && /bin/busybox mknod -m 666 /dev/spec_sync c ${devno%:*} ${devno#*:} || { echo AVR_MM_DRIVERS_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/spec-sync-test || { echo AVR_MM_DRIVERS_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/busybox grep -cE "^(msm_ext_display|sync_fence|msm_hfi_core|msm_mmrm|sps_drv|tz_log_dlkm|qce50_dlkm|qcedev_mod_dlkm|qcrypto_msm_dlkm|hdcp_qseecom_dlkm|qrng_dlkm|smmu_proxy_dlkm|tmecom_intf_dlkm|drbg) " /proc/modules', '[ "$(/bin/busybox grep -cE "^(msm_ext_display|sync_fence|msm_hfi_core|msm_mmrm|sps_drv|tz_log_dlkm|qce50_dlkm|qcedev_mod_dlkm|qcrypto_msm_dlkm|hdcp_qseecom_dlkm|qrng_dlkm|smmu_proxy_dlkm|tmecom_intf_dlkm|drbg) " /proc/modules)" = 14 ] || { echo AVR_MM_DRIVERS_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/busybox grep -q "^name *: stdrng" /proc/crypto || { echo AVR_MM_DRIVERS_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_MM_DRIVERS_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A25 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }; true']
        commands.append('echo AVR_MM_DRIVERS_TESTS_PASS scope=display-deps-securemsm-spec-sync')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.msm_drm_tests:
        sources=[ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/secure-buffer/secure_buffer.ko',ROOT/'ports/gunyah/gh_arm_drv.ko',ROOT/'ports/gunyah/gh_dbl.ko',ROOT/'ports/gunyah/gh_msgq.ko',ROOT/'tests/dma-heaps/avr-gunyah-stub.ko',ROOT/'ports/gunyah-rm/gh_rm_drv.ko',ROOT/'ports/qcom-dma-heaps/mem-prot.ko',ROOT/'ports/qcom-dma-heaps/gh_mem_notifier.ko',ROOT/'ports/qcom-dma-heaps/mem_buf_dev.ko',ROOT/'ports/qcom-iommu-util/qcom_iommu_util.ko',ROOT/'ports/qcom-dma-heaps/msm_dma_iommu_mapping.ko',ROOT/'ports/mm-drivers/sync_fence.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-glink-transport/qcom_glink.ko',ROOT/'ports/qcom-glink-transport/qcom_glink_smem.ko',ROOT/'ports/qcom-rproc-common/qcom_common.ko',ROOT/'ports/qti-fence/msm_hw_fence.ko',ROOT/'ports/qcom-llcc/llcc-qcom.ko',ROOT/'ports/qcom-panel-event/panel_event_notifier.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-debug-diag/qcom_va_minidump.ko',ROOT/'ports/pinctrl-alor-vendor-core/pinctrl-msm.ko',ROOT/'ports/qcom-spmi/spmi-pmic-arb.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/qcom-clock-core/proxy-consumer.ko',ROOT/'ports/qcom-regulators/debug-regulator.ko',ROOT/'ports/qcom-clock-core/gdsc-regulator.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-clock-core/crm-v2.ko',ROOT/'ports/qcom-firststage-misc/icc-clk.ko',ROOT/'ports/qcom-clock-core/clk-qcom.ko',ROOT/'ports/qcom-pas-alor/mdt_loader.ko',ROOT/'ports/mm-drivers/msm_hfi_core.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'out/native-gki/drivers/soc/qcom/qcom_pdr_msg.ko',ROOT/'out/native-gki/drivers/soc/qcom/pdr_interface.ko',ROOT/'ports/qcom-pmic-glink/qti_pmic_glink.ko',ROOT/'ports/qcom-charging/altmode-glink.ko',ROOT/'ports/qcom-usb/redriver.ko',ROOT/'ports/usb-repeater/repeater.ko',ROOT/'ports/qcom-spmi/qti-regmap-debugfs.ko',ROOT/'ports/qcom-display-deps/wcd_usbss_i2c.ko',ROOT/'ports/qcom-usb/dwc3-msm.ko',ROOT/'ports/qcom-display-deps/fsa4480_i2c.ko',ROOT/'ports/drm-display-helper/drm_display_helper.ko',ROOT/'ports/mm-drivers/msm_ext_display.ko',ROOT/'ports/qcom-tee/qseecom_proxy.ko',ROOT/'ports/qcom-tee/si_core_module.ko',ROOT/'ports/securemsm/smcinvoke_dlkm.ko',ROOT/'ports/securemsm/hdcp_qseecom_dlkm.ko',ROOT/'ports/qcom-display-deps/gh_irq_lend.ko',ROOT/'ports/qcom-rpmh/cmd-db.ko',ROOT/'ports/qcom-rpmh/qcom_rpmh.ko',ROOT/'tests/msm-drm/avr-synx-stub.ko',ROOT/'ports/msm-drm/msm_drm.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MSM_DRM_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['[ -d /sys/bus/platform/drivers/msm_drm ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/sde_rsc ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/drm_dsi_ctrl ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/dsi_phy ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/msm-dsi-display ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/msm-dp-display ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/sde_wb ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/msmdrm_smmu ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '[ -d /sys/bus/platform/drivers/dp_pll ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/busybox ls /sys/bus/platform/drivers | /bin/busybox grep -cE "^(msm_drm|sde_rsc|drm_dsi_ctrl|dsi_phy|msm-dsi-display|msm-dp-display|sde_wb|msmdrm_smmu|dp_pll)$"', '/bin/busybox grep -cE "^(msm_drm|drm_display_helper|gh_irq_lend|fsa4480_i2c|wcd_usbss_i2c|msm_hfi_core|sync_fence|msm_ext_display|hdcp_qseecom_dlkm) " /proc/modules', '[ "$(/bin/busybox grep -cE "^(msm_drm|drm_display_helper|gh_irq_lend|fsa4480_i2c|wcd_usbss_i2c|msm_hfi_core|sync_fence|msm_ext_display|hdcp_qseecom_dlkm) " /proc/modules)" = 9 ] || { echo AVR_MSM_DRM_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_MSM_DRM_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A30 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }; true']
        commands.append('echo AVR_MSM_DRM_TESTS_PASS scope=msm-drm-display-stack-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.audio_conn_tests:
        sources=[ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-glink-transport/qcom_glink.ko',ROOT/'ports/qcom-glink-transport/qcom_glink_smem.ko',ROOT/'ports/qcom-rproc-common/qcom_common.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'out/native-gki/drivers/soc/qcom/qcom_pdr_msg.ko',ROOT/'out/native-gki/drivers/soc/qcom/pdr_interface.ko',ROOT/'ports/audio-kernel/q6_pdr_dlkm.ko',ROOT/'ports/audio-kernel/q6_notifier_dlkm.ko',ROOT/'ports/audio-kernel/snd_event_dlkm.ko',ROOT/'ports/audio-kernel/gpr_dlkm.ko',ROOT/'ports/audio-kernel/spf_core_dlkm.ko',ROOT/'ports/audio-kernel/adsp_loader_dlkm.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/audio-kernel/audpkt_ion_dlkm.ko',ROOT/'ports/audio-kernel/audio_pkt_dlkm.ko',ROOT/'ports/audio-kernel/audio_prm_dlkm.ko',ROOT/'ports/audio-kernel/aw882xx_dlkm.ko',ROOT/'ports/mm-drivers/msm_ext_display.ko',ROOT/'ports/audio-kernel/hdmi_dlkm.ko',ROOT/'ports/pinctrl-alor-vendor-core/pinctrl-msm.ko',ROOT/'ports/audio-kernel/wcd_core_dlkm.ko',ROOT/'ports/audio-kernel/swr_dlkm.ko',ROOT/'ports/audio-kernel/swr_ctrl_dlkm.ko',ROOT/'ports/audio-kernel/lpass_bt_swr_dlkm.ko',ROOT/'ports/qcom-spmi/qti-regmap-debugfs.ko',ROOT/'ports/audio-kernel/lpass_cdc_dlkm.ko',ROOT/'ports/audio-kernel/lpass_cdc_rx_macro_dlkm.ko',ROOT/'ports/audio-kernel/lpass_cdc_tx_macro_dlkm.ko',ROOT/'ports/audio-kernel/lpass_cdc_va_macro_dlkm.ko',ROOT/'ports/audio-kernel/lpass_cdc_wsa2_macro_dlkm.ko',ROOT/'ports/audio-kernel/lpass_cdc_wsa_macro_dlkm.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/qcom-display-deps/fsa4480_i2c.ko',ROOT/'ports/qcom-display-deps/wcd_usbss_i2c.ko',ROOT/'ports/audio-kernel/wcd9xxx_dlkm.ko',ROOT/'ports/audio-kernel/sdca_registers_dlkm.ko',ROOT/'ports/audio-kernel/mbhc_dlkm.ko',ROOT/'ports/audio-kernel/wcd9378_dlkm.ko',ROOT/'ports/audio-kernel/wcd939x_slave_dlkm.ko',ROOT/'ports/audio-kernel/wcd939x_dlkm.ko',ROOT/'ports/audio-kernel/wcd937x_dlkm.ko',ROOT/'ports/audio-kernel/wsa883x_dlkm.ko',ROOT/'ports/audio-kernel/wsa884x_dlkm.ko',ROOT/'ports/audio-kernel/machine_dlkm.ko',ROOT/'ports/audio-kernel/pinctrl_lpi_dlkm.ko',ROOT/'ports/audio-kernel/q6_dlkm.ko',ROOT/'ports/audio-kernel/qmp_dlkm.ko',ROOT/'ports/audio-kernel/simple_amp_dlkm.ko',ROOT/'ports/audio-kernel/stub_dlkm.ko',ROOT/'ports/audio-kernel/swr_dmic_dlkm.ko',ROOT/'ports/qcom-pmic-glink/qti_pmic_glink.ko',ROOT/'ports/qcom-panel-event/panel_event_notifier.ko',ROOT/'ports/qcom-charging/qti_battery_charger.ko',ROOT/'ports/audio-kernel/swr_haptics_dlkm.ko',ROOT/'ports/audio-kernel/wcd9378_slave_dlkm.ko',ROOT/'ports/audio-kernel/wcd937x_slave_dlkm.ko',ROOT/'ports/audio-kernel/wcd938x_dlkm.ko',ROOT/'ports/audio-kernel/wcd938x_slave_dlkm.ko',ROOT/'ports/audio-kernel/wsa885x_i2c_dlkm.ko',ROOT/'ports/qcom-wlan-deps/smem-mailbox.ko',ROOT/'ports/wlan-platform/cnss_utils.ko',ROOT/'ports/qcom-remoteproc-services/qcom_aoss.ko',ROOT/'out/native-gki/net/rfkill/rfkill.ko',ROOT/'ports/bt-kernel/btpower.ko',ROOT/'ports/bt-kernel/btfmcodec.ko',ROOT/'ports/bt-kernel/bt_fm_swr.ko',ROOT/'ports/qcom-slimbus/slimbus.ko',ROOT/'ports/bt-kernel/btfm_slim_codec.ko',ROOT/'ports/bt-kernel/radio-i2c-rtc6226-qca.ko',ROOT/'ports/bt-kernel/spi_cnss_proto.ko',ROOT/'ports/qcom-slimbus/bam_dma.ko',ROOT/'ports/qcom-slimbus/slim-qcom-ngd-ctrl.ko',ROOT/'ports/qcom-wlan-deps/qcom_ramdump.ko',ROOT/'ports/wlan-platform/cnss_nl.ko',ROOT/'ports/wlan-platform/cnss_plat_ipc_qmi_svc.ko',ROOT/'ports/wlan-platform/cnss_prealloc.ko',ROOT/'ports/wlan-platform/wlan_firmware_service.ko',ROOT/'ports/wlan-platform/icnss2.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_AUDIO_CONN_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox ls /sys/kernel/qcom_rproc/ || { echo AVR_AUDIO_CONN_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', 'N=$(/bin/busybox wc -l < /proc/modules); echo modules=$N', '/bin/busybox ls /sys/bus/platform/drivers | /bin/busybox grep -ciE "icnss|bt_power|btfm|lpass|wcd|swr|slim|q6|gpr|spf|audio|msm_bt|pinctrl-lpi|bam"', '/bin/busybox ls /sys/class/sound 2>/dev/null; true', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_AUDIO_CONN_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A30 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }; true']
        commands.append('echo AVR_AUDIO_CONN_TESTS_PASS scope=audio-slimbus-bt-wlanplatform-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.media_data_tests:
        sources=[ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/secure-buffer/secure_buffer.ko',ROOT/'ports/gunyah/gh_arm_drv.ko',ROOT/'ports/gunyah/gh_dbl.ko',ROOT/'ports/gunyah/gh_msgq.ko',ROOT/'tests/dma-heaps/avr-gunyah-stub.ko',ROOT/'ports/gunyah-rm/gh_rm_drv.ko',ROOT/'ports/qcom-dma-heaps/mem-prot.ko',ROOT/'ports/qcom-dma-heaps/gh_mem_notifier.ko',ROOT/'ports/qcom-dma-heaps/mem_buf_dev.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-clock-core/crm-v2.ko',ROOT/'ports/qcom-clock-core/proxy-consumer.ko',ROOT/'ports/qcom-regulators/debug-regulator.ko',ROOT/'ports/qcom-clock-core/gdsc-regulator.ko',ROOT/'ports/qcom-firststage-misc/icc-clk.ko',ROOT/'ports/qcom-clock-core/clk-qcom.ko',ROOT/'ports/mm-drivers/msm-mmrm.ko',ROOT/'ports/qcom-debug-diag/qcom_va_minidump.ko',ROOT/'ports/qcom-rpmh/cmd-db.ko',ROOT/'ports/qcom-rpmh/qcom_rpmh.ko',ROOT/'ports/qcom-tee/qseecom_proxy.ko',ROOT/'ports/qcom-tee/si_core_module.ko',ROOT/'ports/securemsm/smcinvoke_dlkm.ko',ROOT/'ports/qcom-dma-heaps/msm_dma_iommu_mapping.ko',ROOT/'ports/qcom-dma-heaps/deferred-free-helper.ko',ROOT/'ports/qcom-dma-heaps/qcom_dma_heaps.ko',ROOT/'ports/securemsm/smmu_proxy_dlkm.ko',ROOT/'ports/qcom-iommu-util/qcom_iommu_util.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/qcom-llcc/llcc-qcom.ko',ROOT/'ports/qcom-glink-transport/qcom_glink.ko',ROOT/'ports/qcom-glink-transport/qcom_glink_smem.ko',ROOT/'ports/qcom-rproc-common/qcom_common.ko',ROOT/'ports/qti-fence/msm_hw_fence.ko',ROOT/'ports/qcom-pas-alor/mdt_loader.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'out/native-gki/drivers/soc/qcom/qcom_pdr_msg.ko',ROOT/'out/native-gki/drivers/soc/qcom/pdr_interface.ko',ROOT/'ports/qcom-pmic-glink/qti_pmic_glink.ko',ROOT/'ports/qcom-panel-event/panel_event_notifier.ko',ROOT/'ports/qcom-charging/qti_battery_charger.ko',ROOT/'ports/qcom-leds/qcom-spmi-wled.ko',ROOT/'ports/qcom-leds/leds-qcom-flash.ko',ROOT/'tests/msm-drm/avr-synx-stub.ko',ROOT/'ports/camera-kernel/camera.ko',ROOT/'ports/dataipa/gsim.ko',ROOT/'ports/qcom-remoteproc-services/qcom_aoss.ko',ROOT/'ports/qcom-wlan-deps/qcom_ramdump.ko',ROOT/'ports/dataipa/rmnet_mem.ko',ROOT/'ports/qcom-usb/redriver.ko',ROOT/'ports/usb-repeater/repeater.ko',ROOT/'ports/qcom-spmi/qti-regmap-debugfs.ko',ROOT/'ports/qcom-display-deps/wcd_usbss_i2c.ko',ROOT/'ports/qcom-usb/dwc3-msm.ko',ROOT/'ports/qcom-usb-gadget/usb_f_gsi.ko',ROOT/'ports/dataipa/ipam.ko',ROOT/'ports/dataipa/ipanetm.ko',ROOT/'ports/datarmnet/rmnet_ctl.ko',ROOT/'ports/datarmnet/rmnet_core.ko',ROOT/'ports/datarmnet/rmnet_aps.ko',ROOT/'ports/datarmnet/rmnet_offload.ko',ROOT/'ports/datarmnet/rmnet_perf.ko',ROOT/'ports/datarmnet/rmnet_perf_tether.ko',ROOT/'ports/datarmnet/rmnet_sch.ko',ROOT/'ports/sched-walt/sched-walt.ko',ROOT/'ports/datarmnet/rmnet_shs.ko',ROOT/'ports/datarmnet/rmnet_wlan.ko',ROOT/'ports/dsp-kernel/frpc-adsprpc.ko',ROOT/'ports/eva-kernel/msm-eva.ko',ROOT/'ports/qcom-eom/eom_driver.ko',ROOT/'ports/qcom-eom/pci_phy_access.ko',ROOT/'ports/qcom-leds/leds-qcom-lpg.ko',ROOT/'ports/qcom-leds/leds-qpnp-vibrator-ldo.ko',ROOT/'ports/qcom-leds/qcom-hv-haptics.ko',ROOT/'ports/qcom-pm-telemetry/cpuss_telemetry.ko',ROOT/'ports/qcom-usb-gadget/moto_f_usbnet.ko',ROOT/'ports/qcom-usb-gadget/usb_f_ccid.ko',ROOT/'ports/qcom-usb-gadget/usb_f_cdev.ko',ROOT/'ports/qcom-usb-gadget/usb_f_qdss.ko',ROOT/'ports/video-driver/msm_video.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ' midpoint_freq=2500000 sw_cycle_counter=1' if source.name=='sched-walt.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MEDIA_DATA_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox grep -cE "^(usb_f_gsi|usb_f_cdev|usb_f_ccid|usb_f_qdss|moto_f_usbnet|gsim|ipam|ipanetm|rmnet_mem|rmnet_core|rmnet_ctl|rmnet_aps|rmnet_offload|rmnet_perf|rmnet_perf_tether|rmnet_sch|rmnet_shs|rmnet_wlan|eom_driver|pci_phy_access|cpuss_telemetry|leds_qcom_flash|qcom_spmi_wled|leds_qcom_lpg|leds_qpnp_vibrator_ldo|qcom_hv_haptics|camera|msm_video|msm_eva|frpc_adsprpc) " /proc/modules', '[ "$(/bin/busybox grep -cE "^(usb_f_gsi|usb_f_cdev|usb_f_ccid|usb_f_qdss|moto_f_usbnet|gsim|ipam|ipanetm|rmnet_mem|rmnet_core|rmnet_ctl|rmnet_aps|rmnet_offload|rmnet_perf|rmnet_perf_tether|rmnet_sch|rmnet_shs|rmnet_wlan|eom_driver|pci_phy_access|cpuss_telemetry|leds_qcom_flash|qcom_spmi_wled|leds_qcom_lpg|leds_qpnp_vibrator_ldo|qcom_hv_haptics|camera|msm_video|msm_eva|frpc_adsprpc) " /proc/modules)" = 30 ] || { echo AVR_MEDIA_DATA_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/busybox ls /sys/class/net | /bin/busybox tr "\\n" " "; echo', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_MEDIA_DATA_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A30 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }; true']
        commands.append('echo AVR_MEDIA_DATA_TESTS_PASS scope=gadget-ipa-rmnet-camera-video-eva-fastrpc-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.soc_misc_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/gunyah-misc/gh_ctrl.ko',ROOT/'ports/gunyah/gh_dbl.ko',ROOT/'ports/gunyah/gh_arm_drv.ko',ROOT/'ports/gunyah/gh_msgq.ko',ROOT/'out/native-gki/drivers/firmware/qcom/qcom_pas.ko',ROOT/'tests/qcom-tee/fake-scm/qcom-scm.ko',ROOT/'ports/secure-buffer/secure_buffer.ko',ROOT/'tests/dma-heaps/avr-gunyah-stub.ko',ROOT/'ports/gunyah-rm/gh_rm_drv.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-socinfo/socinfo.ko',ROOT/'ports/qcom-perf-misc/gh_vcpu_mgr.ko',ROOT/'ports/gunyah-misc/gh_panic_notifier.ko',ROOT/'ports/gunyah-misc/gh_rm_booster.ko',ROOT/'ports/pinctrl-alor-vendor-core/pinctrl-msm.ko',ROOT/'ports/qcom-dma-heaps/gh_mem_notifier.ko',ROOT/'ports/gunyah-misc/gh_tlmm_vm_mem_access.ko',ROOT/'ports/gunyah-misc/gunyah_trace.ko',ROOT/'ports/gunyah-misc/hvc_gunyah.ko',ROOT/'out/native-gki/net/qrtr/qrtr.ko',ROOT/'ports/gunyah-misc/qrtr-gunyah.ko',ROOT/'ports/qcom-rpmh/cmd-db.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-clock-core/crm-v2.ko',ROOT/'ports/qcom-rpmh/qcom_rpmh.ko',ROOT/'ports/qcom-dcvs/dcvs_fp.ko',ROOT/'ports/qcom-dcvs/qcom-dcvs.ko',ROOT/'ports/sched-walt/sched-walt.ko',ROOT/'ports/qcom-dcvs-misc/bwmon.ko',ROOT/'ports/qcom-cpucp-scmi/qcom_scmi_client.ko',ROOT/'ports/qcom-dcvs-misc/mpam.ko',ROOT/'ports/qcom-dcvs-misc/cpu_mpam.ko',ROOT/'ports/qcom-dcvs-misc/llcc_heuristics.ko',ROOT/'ports/qcom-cpucp-scmi/cpu_phys_log_map.ko',ROOT/'ports/qcom-perf-misc/qcom_llcc_pmu.ko',ROOT/'ports/qcom-perf-misc/qcom-pmu-lib.ko',ROOT/'ports/qcom-dcvs-misc/memlat.ko',ROOT/'ports/qcom-dcvs-misc/mpam_msc.ko',ROOT/'ports/qcom-dcvs-misc/mpam_msc_slc.ko',ROOT/'ports/qcom-dcvs-misc/platform_mpam.ko',ROOT/'ports/qcom-dcvs-misc/qcom-cpufreq-hw-debug.ko',ROOT/'ports/qcom-dcvs-misc/qcom_cache_allocation.ko',ROOT/'ports/qcom-remoteproc-services/qcom_aoss.ko',ROOT/'ports/qcom-dcvs-misc/qcom_fdcvs.ko',ROOT/'ports/qcom-dcvs-misc/slc_mpam.ko',ROOT/'ports/qcom-debug-misc/core_hang_detect.ko',ROOT/'ports/qcom-debug-misc/dcc_v2.ko',ROOT/'ports/qcom-debug-misc/dmesg_dumper.ko',ROOT/'ports/qcom-debug-misc/f_fs_ipc_log.ko',ROOT/'ports/qcom-debug-misc/hung_task_enh.ko',ROOT/'ports/qcom-debug-misc/mem-offline.ko',ROOT/'ports/qcom-glink-transport/qcom_glink.ko',ROOT/'ports/qcom-glink-transport/qcom_glink_smem.ko',ROOT/'ports/qcom-rproc-common/qcom_common.ko',ROOT/'ports/qcom-wlan-deps/qcom_ramdump.ko',ROOT/'ports/qcom-debug-misc/microdump_collector.ko',ROOT/'ports/qcom-debug-misc/qcom_dynamic_ramoops.ko',ROOT/'ports/qcom-debug-misc/qcom_edac.ko',ROOT/'ports/qcom-debug-misc/rdbg.ko',ROOT/'ports/qcom-clock-core/proxy-consumer.ko',ROOT/'ports/qcom-regulators/debug-regulator.ko',ROOT/'ports/qcom-fixed-regulator/qti-fixed-regulator.ko',ROOT/'ports/qcom-pmic-misc/qcom-amoled-regulator.ko',ROOT/'ports/qcom-spmi/qti-regmap-debugfs.ko',ROOT/'ports/qcom-pmic-misc/qcom-i2c-pmic.ko',ROOT/'ports/qcom-pmic-misc/qcom-pmic-ecid.ko',ROOT/'ports/qcom-pmic-misc/qcom_voltage_adj.ko',ROOT/'ports/qcom-pmic-misc/qpnp-lcdb-regulator.ko',ROOT/'out/native-gki/drivers/hwmon/hwmon.ko',ROOT/'ports/qcom-panel-event/panel_event_notifier.ko',ROOT/'ports/qcom-pmic-misc/qti_amoled_ecm.ko',ROOT/'ports/qcom-pmic-misc/spmi-pmic-err-debug.ko',ROOT/'ports/qcom-thermal-misc/lmh_stats.ko',ROOT/'ports/qcom-thermal-misc/qptf.ko',ROOT/'ports/qcom-thermal-misc/qti-ocp-notifier.ko',ROOT/'ports/qcom-thermal-misc/qti_btl.ko',ROOT/'ports/qcom-thermal-misc/qti_dynamic_hw_trip.ko',ROOT/'ports/qcom-thermal-misc/qti_power_telemetry.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'ports/qcom-thermal-misc/qti_qmi_cdev.ko',ROOT/'ports/qcom-thermal-misc/qti_qmi_sensor_v2.ko',ROOT/'ports/qcom-thermal-misc/qti_thermal_vendor_hooks.ko',ROOT/'ports/qcom-thermal-misc/thermal_config.ko',ROOT/'ports/qcom-clock-core/gdsc-regulator.ko',ROOT/'ports/qcom-firststage-misc/icc-clk.ko',ROOT/'ports/qcom-clock-core/clk-qcom.ko',ROOT/'ports/qcom-mhi-pcie/pcie-pdc.ko',ROOT/'ports/qcom-mhi-pcie/pci-msm-drv.ko',ROOT/'ports/qcom-mhi-pcie/mhi.ko',ROOT/'ports/qcom-mhi-pcie/mhi_dev_satellite.ko',ROOT/'ports/qcom-mhi-pcie/mhi_dev_uci.ko',ROOT/'ports/qcom-mhi-pcie/qrtr-mhi.ko',ROOT/'ports/qcom-remoteproc-services/qcom_sysmon.ko',ROOT/'ports/qcom-remoteproc-services/qcom_q6v5.ko',ROOT/'ports/qcom-pas-alor/mdt_loader.ko',ROOT/'ports/qcom-pas-alor/qcom_pil_info.ko',ROOT/'ports/qcom-pas-alor/qcom_q6v5_pas.ko',ROOT/'ports/qcom-soc-misc2/adsp_sleepmon.ko',ROOT/'ports/qcom-soc-misc2/cdsprm.ko',ROOT/'ports/qcom-soc-misc2/heap_mem_ext_v01.ko',ROOT/'ports/geni-bus/msm_gpi.ko',ROOT/'ports/qcom-soc-misc2/i3c-master-msm-geni.ko',ROOT/'ports/qcom-soc-misc2/lt9611uxc.ko',ROOT/'ports/qcom-soc-misc2/msm_geni_serial.ko',ROOT/'ports/qcom-soc-misc2/msm_memshare.ko',ROOT/'ports/qcom-soc-misc2/msm_sharedmem.ko',ROOT/'ports/qcom-soc-misc2/phy-qcom-m31-eusb2.ko',ROOT/'ports/qcom-soc-misc2/phy-qcom-qmp-combo.ko',ROOT/'ports/qcom-soc-misc2/qsee_ipc_irq_bridge.ko',ROOT/'ports/qcom-soc-misc2/qti_ptd_share.ko',ROOT/'ports/qcom-soc-misc2/snd-usb-audio-qmi.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ' midpoint_freq=2500000 sw_cycle_counter=1' if source.name=='sched-walt.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SOC_MISC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['[ "$(/bin/busybox grep -cE "^(gh_ctrl|gh_panic_notifier|gh_rm_booster|gh_tlmm_vm_mem_access|gunyah_trace|hvc_gunyah|qrtr_gunyah|bwmon|mpam|cpu_mpam|llcc_heuristics|memlat|mpam_msc|mpam_msc_slc|platform_mpam|qcom_cpufreq_hw_debug|qcom_cache_allocation|qcom_fdcvs|slc_mpam|core_hang_detect|dcc_v2|dmesg_dumper|f_fs_ipc_log|hung_task_enh|mem_offline|microdump_collector|qcom_dynamic_ramoops|qcom_edac|rdbg|qti_fixed_regulator|qcom_amoled_regulator|qcom_i2c_pmic|qcom_pmic_ecid|qcom_voltage_adj|qpnp_lcdb_regulator|qti_amoled_ecm|spmi_pmic_err_debug|lmh_stats|qptf|qti_ocp_notifier|qti_btl|qti_dynamic_hw_trip|qti_power_telemetry|qti_qmi_cdev|qti_qmi_sensor_v2|qti_thermal_vendor_hooks|thermal_config|mhi|mhi_dev_satellite|mhi_dev_uci|pci_msm_drv|pcie_pdc|qrtr_mhi|adsp_sleepmon|cdsprm|heap_mem_ext_v01|i3c_master_msm_geni|lt9611uxc|msm_geni_serial|msm_memshare|msm_sharedmem|phy_qcom_m31_eusb2|phy_qcom_qmp_combo|qsee_ipc_irq_bridge|qti_ptd_share|snd_usb_audio_qmi) " /proc/modules)" = 66 ] || { echo AVR_SOC_MISC_STEP_FAIL; /bin/busybox dmesg | /bin/busybox tail -40; /bin/busybox poweroff -f; }', '/bin/busybox dmesg | /bin/busybox grep -q "WARNING:\\\\|BUG:\\\\|Oops" && { echo AVR_SOC_MISC_KERNEL_WARN; /bin/busybox dmesg | /bin/busybox grep -A30 "WARNING:\\\\|BUG:"; /bin/busybox poweroff -f; }; true']
        commands.append('echo AVR_SOC_MISC_TESTS_PASS scope=thermal-pmic-debug-gunyah-dcvs-mhi-pcie-misc2-load')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.devfreq_cooling_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qti-devfreq-cdev/qti_devfreq_cdev.ko',ROOT/'tests/devfreq-cooling/avr-devfreq-cooling-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_DEVFREQ_COOLING_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_devfreq_cooling_test','qti_devfreq_cdev']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_DEVFREQ_COOLING_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_DEVFREQ_COOLING_TESTS_PASS scope=native-opp-qos-thermal-private-gpu-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.ddr_cooling_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-ddr-cooling/ddr_cdev.ko',ROOT/'tests/ddr-cooling/avr-ddr-cooling-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_DDR_COOLING_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_ddr_cooling_test','ddr_cdev']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_DDR_COOLING_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_DDR_COOLING_TESTS_PASS scope=native-icc-thermal-recorded-provider-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.qmi_cooling_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'out/native-gki/net/qrtr/qrtr.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'ports/qcom-qmi-cooling/qmi_cooling.ko',ROOT/'tests/qmi-cooling/avr-qmi-cooling-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_QMI_COOLING_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for module in ['avr_qmi_cooling_test','qmi_cooling','qmi_helpers','qrtr']:
            commands.append(f'/bin/busybox rmmod {module} || {{ echo AVR_QMI_COOLING_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_QMI_COOLING_TESTS_PASS scope=native-local-qmi-recorded-peer-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.bcl_soc_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-bcl-soc/bcl_soc.ko',ROOT/'tests/bcl-soc/avr-bcl-soc-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_BCL_SOC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_bcl_soc_test || { echo AVR_BCL_SOC_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('/bin/busybox rmmod bcl_soc || { echo AVR_BCL_SOC_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_BCL_SOC_TESTS_PASS scope=native-supply-thermal-qos-recorded-policy-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.rng_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-rng/qcom-rng.ko',ROOT/'tests/rng/avr-rng-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_RNG_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_rng_test || { echo AVR_RNG_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('/bin/busybox rmmod qcom_rng || { echo AVR_RNG_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_RNG_TESTS_PASS scope=recorded-mmio-native-poll-clock-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.tsens_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-tsens/qcom_tsens.ko',ROOT/'tests/tsens/avr-tsens-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_TSENS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_tsens_test || { echo AVR_TSENS_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('/bin/busybox rmmod qcom_tsens || { echo AVR_TSENS_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_TSENS_TESTS_PASS scope=native-regmap-private-ram-and-driver-registration-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.geni_map_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/geni-map/avr-geni-map-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_GENI_MAP_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_geni_map_test || { echo AVR_GENI_MAP_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_GENI_MAP_TESTS_PASS scope=native-map-with-recorded-faults-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.geni_pm_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','qcom-ipc-logging/qcom_ipc_logging.ko']]+[ROOT/'tests/geni-pm/avr-geni-pm-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_GENI_PM_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_geni_pm_test || { echo AVR_GENI_PM_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_GENI_PM_TESTS_PASS scope=native-runtime-pm-recorded-hardware-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.geni_ring_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','qcom-ipc-logging/qcom_ipc_logging.ko']]+[ROOT/'tests/geni-ring/avr-geni-ring-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_GENI_RING_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_geni_ring_test || { echo AVR_GENI_RING_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_GENI_RING_TESTS_PASS scope=source-ring-native-dma-private-ram-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.geni_bus_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','qcom-ipc-logging/qcom_ipc_logging.ko','geni-bus/msm_gpi.ko','geni-bus/i2c-msm-geni.ko','geni-bus/spi-msm-geni.ko']]+[ROOT/'tests/geni-bus/avr-geni-bus-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_GENI_BUS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_geni_bus_test','spi_msm_geni','i2c_msm_geni','msm_gpi']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_GENI_BUS_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_GENI_BUS_TESTS_PASS scope=native-wrapper-ram-and-helper-lifecycle-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.usb_repeater_pmic_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/usb-repeater/repeater.ko',ROOT/'tests/usb-repeater-pmic/avr-usb-repeater-pmic-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_USB_REPEATER_PMIC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_usb_repeater_pmic_test','repeater']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_USB_REPEATER_PMIC_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_USB_REPEATER_PMIC_TESTS_PASS scope=native-regmap-private-ram-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.usb_repeater_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/usb-repeater/repeater.ko',ROOT/'tests/usb-repeater/avr-usb-repeater-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_USB_REPEATER_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_usb_repeater_test','repeater']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_USB_REPEATER_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_USB_REPEATER_TESTS_PASS scope=real-registry-devres-refcounts-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.goodix_mmap_tests:
        add('modules/avr-goodix-mmap-test.ko',stat.S_IFREG|0o644,(ROOT/'tests/goodix-mmap/avr-goodix-mmap-test.ko').read_bytes())
        add('bin/goodix-mmap-test',stat.S_IFREG|0o755,(ROOT/'tests/goodix-mmap/user-test').read_bytes())
        commands = """/bin/busybox insmod /modules/avr-goodix-mmap-test.ko || { echo AVR_GOODIX_MMAP_LOAD_FAIL; /bin/busybox poweroff -f; }
devno=$(/bin/busybox cat /sys/class/misc/avr_goodix_mmap/dev)
/bin/busybox mknod /dev/avr_goodix_mmap c ${devno%:*} ${devno#*:}
/bin/goodix-mmap-test || { echo AVR_GOODIX_MMAP_USER_FAIL; /bin/busybox poweroff -f; }
/bin/busybox rmmod avr_goodix_mmap_test || { echo AVR_GOODIX_MMAP_UNLOAD_FAIL; /bin/busybox poweroff -f; }
echo AVR_GOODIX_MMAP_TESTS_PASS scope=real-vm-selected-source-only"""
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS',commands.encode())
    if args.mmi_info_native_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','moto-info/mmi_annotate.ko','moto-info/mmi_info.ko']]+[ROOT/'tests/mmi-info-native/avr-mmi-info-native-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MMI_INFO_NATIVE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_mmi_info_native_test || { echo AVR_MMI_INFO_NATIVE_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        for cycle in range(2):
            commands.append('[ -e /sys/storage/type ] && [ -e /proc/bootinfo ] && [ ! -e /sys/ram ] && [ ! -e /proc/unitinfo ] || { echo AVR_MMI_INFO_NATIVE_ENDPOINT_FAIL; /bin/busybox poweroff -f; }')
            commands.append('/bin/busybox rmmod mmi_info || { echo AVR_MMI_INFO_NATIVE_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
            commands.append('[ ! -e /sys/storage ] && [ ! -e /sys/ram ] && [ ! -e /proc/bootinfo ] && [ ! -e /proc/unitinfo ] || { echo AVR_MMI_INFO_NATIVE_CLEANUP_FAIL; /bin/busybox poweroff -f; }')
            if cycle==0:
                commands.append('/bin/busybox insmod /modules/mmi_info.ko || { echo AVR_MMI_INFO_NATIVE_RELOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('/bin/busybox rmmod mmi_annotate || { echo AVR_MMI_INFO_NATIVE_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_MMI_INFO_NATIVE_TESTS_PASS scope=production-modules-real-unready-smem-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.mmi_info_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']+[ROOT/'ports'/name for name in ['qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-minidump/minidump.ko','moto-info/mmi_annotate.ko']]+[ROOT/'tests/mmi-info/avr-mmi-info-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MMI_INFO_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_mmi_info_test','mmi_annotate']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_MMI_INFO_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_MMI_INFO_TESTS_PASS scope=actual-info-source-framework-recorded-smem-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.mmi_annotate_driver_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/mmi-annotate-driver/avr-mmi-annotate-driver-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MMI_ANNOTATE_DRIVER_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_mmi_annotate_driver_test']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_MMI_ANNOTATE_DRIVER_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_MMI_ANNOTATE_DRIVER_TESTS_PASS scope=real-probe-proc-ram-map-recorded-minidump-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.mmi_annotate_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/mmi-annotate/avr-mmi-annotate-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MMI_ANNOTATE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_mmi_annotate_test']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_MMI_ANNOTATE_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_MMI_ANNOTATE_TESTS_PASS scope=source-append-methods-private-ram-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.moto_touch_deps_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/moto-touch-deps/mmi_relay.ko',ROOT/'ports/moto-touch-deps/sensors_class.ko',ROOT/'tests/moto-touch-deps/avr-moto-touch-deps-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_MOTO_TOUCH_DEPS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_moto_touch_deps_test','sensors_class','mmi_relay']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_MOTO_TOUCH_DEPS_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_MOTO_TOUCH_DEPS_TESTS_PASS scope=real-relay-notifier-and-sensor-class-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.panel_event_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-panel-event/panel_event_notifier.ko',ROOT/'tests/panel-event/avr-panel-event-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_PANEL_EVENT_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_panel_event_test','panel_event_notifier']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_PANEL_EVENT_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_PANEL_EVENT_TESTS_PASS scope=real-notifier-api-and-callback-lifetime-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.power_key_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-power-key/reboot-mode.ko',ROOT/'ports/qcom-power-key/qcom-pon.ko',ROOT/'ports/qcom-power-key/pm8941-pwrkey.ko',ROOT/'tests/power-key/avr-power-key-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_POWER_KEY_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_power_key_test','pm8941_pwrkey','qcom_pon','reboot_mode']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_POWER_KEY_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_POWER_KEY_TESTS_PASS scope=native-irq-input-regmap-ram-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.sdam_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-sdam/nvmem_qcom-spmi-sdam.ko',ROOT/'tests/sdam/avr-sdam-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SDAM_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_sdam_test','nvmem_qcom_spmi_sdam']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_SDAM_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_SDAM_TESTS_PASS scope=factory-callbacks-native-nvmem-regmap-ram-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.rtc_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-rtc/rtc-pm8xxx.ko',ROOT/'tests/rtc/avr-rtc-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_RTC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_rtc_test','rtc_pm8xxx']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_RTC_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_RTC_TESTS_PASS scope=native-callbacks-real-regmap-ram-bus-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.pas_rails_tests or args.pas_proxy_tests or args.pas_images_tests or args.pas_driver_tests:
        kind='pas-driver' if args.pas_driver_tests else 'pas-images' if args.pas_images_tests else 'pas-proxy' if args.pas_proxy_tests else 'pas-rails'
        if args.pas_driver_tests:
            for directory in ['lib','lib/firmware','lib/firmware/avr-test']:
                add(directory,stat.S_IFDIR|0o755)
            manifest=json.loads((ROOT/'reports/soccp-loader-stage41-inputs.json').read_text())
            for row in manifest['files']:
                path=ROOT/row['file'];raw=path.read_bytes()
                if hashlib.sha256(raw).hexdigest()!=row['sha256']:
                    raise ValueError('Captured firmware changed: '+str(path))
                add('lib/firmware/avr-test/'+path.name,stat.S_IFREG|0o644,raw)
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko']
        sources += firmware_modules()
        sources += [ROOT/'out/native-gki'/p for p in ['net/qrtr/qrtr.ko','drivers/soc/qcom/qmi_helpers.ko']]
        sources += [ROOT/'ports'/p for p in [
            'qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko',
            'qcom-glink-transport/rpmsg_ctrl.ko','qcom-glink-transport/qcom_glink.ko','qcom-glink-transport/qcom_glink_smem.ko',
            'qcom-rproc-common/qcom_common.ko','qcom-remoteproc-services/qcom_aoss.ko',
            'qcom-remoteproc-services/qcom_sysmon.ko','qcom-remoteproc-services/qcom_q6v5.ko',
            'qcom-pas-alor/mdt_loader.ko','qcom-pas-alor/qcom_pil_info.ko','qcom-pas-alor/qcom_q6v5_pas.ko']]
        sources.append(ROOT/'tests'/kind/('avr-'+kind+'-test.ko'))
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            if source.name=='avr-pas-driver-test.ko' and args.pas_critical:
                params=' critical_mode='+('1' if args.pas_critical=='auth' else '2')
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_PAS_RAILS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_'+kind.replace('-','_')+'_test','qcom_q6v5_pas','qcom_pil_info','mdt_loader','qcom_q6v5','qcom_sysmon','qcom_aoss']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_PAS_RAILS_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo '+('AVR_PAS_DRIVER_TESTS_PASS scope=driver-methods-with-recorded-primitives-only' if args.pas_driver_tests else 'AVR_PAS_IMAGES_TESTS_PASS scope=recorded-image-ownership-and-registration-only' if args.pas_images_tests else 'AVR_PAS_PROXY_TESTS_PASS scope=recorded-proxy-ownership-and-registration-only' if args.pas_proxy_tests else 'AVR_PAS_RAILS_TESTS_PASS scope=recorded-regulator-state-machine-and-registration-only'))
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.remoteproc_services_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'out/native-gki/net/qrtr/qrtr.ko',ROOT/'out/native-gki/drivers/soc/qcom/qmi_helpers.ko',ROOT/'ports/qcom-hwspinlock/qcom_hwspinlock.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-remoteproc-services/qcom_aoss.ko',ROOT/'ports/qcom-glink-transport/qcom_glink.ko',ROOT/'ports/qcom-glink-transport/qcom_glink_smem.ko',ROOT/'ports/qcom-rproc-common/qcom_common.ko',ROOT/'tests/remoteproc-services/avr-remoteproc-services-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_REMOTEPROC_SERVICES_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_remoteproc_services_test || { echo AVR_REMOTEPROC_SERVICES_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        for name in ['qcom_sysmon','qcom_q6v5']:
            source=ROOT/'ports/qcom-remoteproc-services'/(name+'.ko')
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            commands.append(f'/bin/busybox insmod /modules/{source.name} || {{ echo AVR_REMOTEPROC_SERVICES_REAL_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['qcom_q6v5','qcom_sysmon','qcom_aoss']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_REMOTEPROC_SERVICES_REAL_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_REMOTEPROC_SERVICES_TESTS_PASS scope=native-callbacks-local-qmi-and-driver-registration-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.smem_state_tests:
        commands=[]
        for source in [ROOT/'out/native-gki/lib/kunit/kunit.ko', ROOT/'tests/smem-state/avr-smem-state-test.ko']:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SMEM_STATE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_smem_state_test || { echo AVR_SMEM_STATE_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_SMEM_STATE_TESTS_PASS scope=native-state-api-and-concurrent-teardown-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.smp2p_tests:
        commands=[]
        for source in [ROOT/'out/native-gki/lib/kunit/kunit.ko', ROOT/'ports/qcom-hwspinlock/qcom_hwspinlock.ko', ROOT/'ports/qcom-smem/smem.ko', ROOT/'tests/smp2p/avr-smp2p-test.ko']:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SMP2P_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_smp2p_test || { echo AVR_SMP2P_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        source=ROOT/'ports/qcom-smp2p/smp2p.ko'
        add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
        for verb in ['insmod /modules/smp2p.ko', 'rmmod smp2p']:
            commands.append('/bin/busybox '+verb+' || { echo AVR_SMP2P_REAL_LIFECYCLE_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_SMP2P_TESTS_PASS scope=ram-state-and-driver-registration-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.ipcc_tests:
        commands=[]
        for source in [ROOT/'out/native-gki/lib/kunit/kunit.ko', ROOT/'tests/ipcc/avr-ipcc-test.ko']:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_IPCC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_ipcc_test || { echo AVR_IPCC_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        source=ROOT/'ports/qcom-ipcc/qcom-ipcc.ko'
        add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
        for verb in ['insmod /modules/qcom-ipcc.ko', 'rmmod qcom_ipcc']:
            commands.append('/bin/busybox '+verb+' || { echo AVR_IPCC_REAL_LIFECYCLE_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_IPCC_TESTS_PASS scope=ram-registers-and-driver-registration-only')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.glink_transport_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-hwspinlock/qcom_hwspinlock.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-glink-transport/rpmsg_ctrl.ko',ROOT/'tests/glink-transport/avr-glink-transport-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_GLINK_TRANSPORT_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_glink_transport_test || { echo AVR_GLINK_TRANSPORT_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        for name in ['qcom_glink','qcom_glink_smem']:
            source=ROOT/'ports/qcom-glink-transport'/(name+'.ko')
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            commands.append(f'/bin/busybox insmod /modules/{source.name} || {{ echo AVR_GLINK_TRANSPORT_REAL_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['qcom_glink_smem','qcom_glink','rpmsg_ctrl']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_GLINK_TRANSPORT_REAL_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['echo AVR_GLINK_TRANSPORT_TESTS_PASS scope=ram-ring-wire-and-driver-registration-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.rproc_gate_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-hwspinlock/qcom_hwspinlock.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-glink-transport/rpmsg_ctrl.ko',ROOT/'ports/qcom-glink-transport/qcom_glink.ko',ROOT/'ports/qcom-glink-transport/qcom_glink_smem.ko',ROOT/'tests/rproc-common/avr-rproc-gate-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_RPROC_GATE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox rmmod avr_rproc_gate_test || { echo AVR_RPROC_GATE_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod qcom_glink_smem || { echo AVR_RPROC_GATE_SMEM_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod qcom_glink || { echo AVR_RPROC_GATE_CORE_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod rpmsg_ctrl || { echo AVR_RPROC_GATE_CTRL_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     'echo AVR_RPROC_GATE_TESTS_PASS scope=missing-or-invalid-transport-error-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.pmic_ipc_tests or args.pmic_glink_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko'] + [ROOT/'out/native-gki'/name for name in [
            'net/qrtr/qrtr.ko','net/qrtr/qrtr-smd.ko','drivers/soc/qcom/qmi_helpers.ko',
            'drivers/soc/qcom/qcom_pdr_msg.ko','drivers/soc/qcom/pdr_interface.ko',
        ]] + [ROOT/'tests/pmic-ipc/avr-qmi-test.ko']
        if args.pmic_glink_tests:
            sources += [ROOT/'ports'/name for name in [
                'qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko',
                'qcom-glink-transport/rpmsg_ctrl.ko','qcom-glink-transport/qcom_glink.ko',
                'qcom-glink-transport/qcom_glink_smem.ko','qcom-rproc-common/qcom_common.ko','qcom-minidump/minidump.ko',
                'qcom-ipc-logging/qcom_ipc_logging.ko',
            ]] + [ROOT/'tests/pmic-glink/avr-pmic-glink-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_PMIC_IPC_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('bin/qrtr-local-test',stat.S_IFREG|0o755,(ROOT/'tests/pmic-ipc/qrtr-local-test').read_bytes())
        commands += ['/bin/qrtr-local-test || { echo AVR_PMIC_IPC_SOCKET_FAIL; /bin/busybox poweroff -f; }']
        if args.pmic_glink_tests:
            commands.append('/bin/busybox rmmod avr_pmic_glink_test || { echo AVR_PMIC_GLINK_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
            source=ROOT/'ports/qcom-pmic-glink/qti_pmic_glink.ko'
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            commands += ['/bin/busybox insmod /modules/qti_pmic_glink.ko || { echo AVR_PMIC_GLINK_REAL_LOAD_FAIL; /bin/busybox poweroff -f; }',
                         '/bin/busybox rmmod qti_pmic_glink || { echo AVR_PMIC_GLINK_REAL_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                         'echo AVR_PMIC_GLINK_REAL_DRIVER_LIFECYCLE_PASS']
        for name in ['avr_qmi_test','pdr_interface','qcom_pdr_msg','qmi_helpers','qrtr_smd','qrtr']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_PMIC_IPC_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['echo AVR_PMIC_GLINK_TESTS_PASS scope=source-callbacks-and-native-local-ipc-only' if args.pmic_glink_tests else 'echo AVR_PMIC_IPC_TESTS_PASS scope=native-local-ipc-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.adc_thermal_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/qcom-hwspinlock/qcom_hwspinlock.ko',ROOT/'ports/qcom-smem/smem.ko',ROOT/'ports/qcom-minidump/minidump.ko',ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',ROOT/'ports/qcom-adc-thermal/qcom-vadc-common.ko',ROOT/'ports/qcom-adc-thermal/qcom-spmi-temp-alarm.ko',ROOT/'tests/adc-thermal/avr-adc-thermal-test.ko',ROOT/'tests/adc-thermal/avr-adc-driver-test.ko',ROOT/'tests/adc-thermal/avr-temp-alarm-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_ADC_THERMAL_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_temp_alarm_test','avr_adc_driver_test','avr_adc_thermal_test','qcom_spmi_temp_alarm']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_ADC_THERMAL_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        source=ROOT/'ports/qcom-adc-thermal/qcom-spmi-adc5-gen3.ko'
        add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
        commands += ['/bin/busybox insmod /modules/qcom-spmi-adc5-gen3.ko || { echo AVR_ADC_THERMAL_REAL_LOAD_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod qcom_spmi_adc5_gen3 || { echo AVR_ADC_THERMAL_REAL_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod qcom_vadc_common || { echo AVR_ADC_THERMAL_COMMON_UNLOAD_FAIL; /bin/busybox poweroff -f; }']
        commands += ['echo AVR_ADC_THERMAL_TESTS_PASS scope=conversion-and-module-lifecycle-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.pmic_gpio_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'ports/pmic-pinctrl/pinctrl-spmi-gpio.ko',ROOT/'ports/pmic-pinctrl/pinctrl-spmi-mpp.ko',ROOT/'tests/pmic-pinctrl/avr-pmic-gpio-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_PMIC_GPIO_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_pmic_gpio_test','pinctrl_spmi_mpp','pinctrl_spmi_gpio']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_PMIC_GPIO_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['echo AVR_PMIC_GPIO_TESTS_PASS scope=callback-source-on-ram-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.spmi_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko'] + [ROOT/'ports/qcom-spmi'/name for name in [
            'qti-regmap-debugfs.ko','spmi-pmic-arb.ko','spmi-pmic-arb-debug.ko','qcom-spmi-pmic.ko',
        ]] + [ROOT/'tests/spmi/avr-spmi-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SPMI_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += [
            '/bin/busybox mkdir -p /sys/kernel/debug',
            '/bin/busybox mount -t debugfs debugfs /sys/kernel/debug || { echo AVR_SPMI_DEBUGFS_FAIL; /bin/busybox poweroff -f; }',
            'dir=/sys/kernel/debug/regmap/avr_spmi_ram-ram; f="$dir/data"',
            '/bin/busybox cat "$f" > /tmp/ram-data || { echo AVR_SPMI_DATA_READ_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox grep -qi 13572468 /tmp/ram-data || { echo AVR_SPMI_DATA_VALUE_FAIL; /bin/busybox poweroff -f; }',
            'if echo deadbeef > "$f"; then echo AVR_SPMI_READONLY_FAIL; /bin/busybox poweroff -f; fi',
            'echo 0x10 > "$dir/address" || { echo AVR_SPMI_ADDRESS_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox cat "$f" > /tmp/ram-data || { echo AVR_SPMI_OFFSET_READ_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox grep -qi 739201a5 /tmp/ram-data || { echo AVR_SPMI_OFFSET_VALUE_FAIL; /bin/busybox poweroff -f; }',
            'exec 9<"$f"',
            'echo 1 > /sys/module/avr_spmi_test/parameters/drop_debug || { echo AVR_SPMI_DEBUG_REMOVE_FAIL; /bin/busybox poweroff -f; }',
            'if /bin/busybox cat <&9; then echo AVR_SPMI_STALE_READ_FAIL; /bin/busybox poweroff -f; fi',
            'exec 9<&-',
            'test -f "$dir/registers" || { echo AVR_SPMI_CORE_FILES_FAIL; /bin/busybox poweroff -f; }']
        for name in ['avr_spmi_test','qcom_spmi_pmic','spmi_pmic_arb_debug','spmi_pmic_arb','qti_regmap_debugfs']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_SPMI_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['echo AVR_SPMI_TESTS_PASS scope=ram-regmap-and-native-framework-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.interconnect_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko'] + [ROOT/'ports'/name for name in [
            'qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko','qcom-socinfo/socinfo.ko',
            'qcom-clock-core/crm-v2.ko','qcom-rpmh/cmd-db.ko','qcom-rpmh/qcom_rpmh.ko',
            'qcom-interconnect/icc-debug.ko','qcom-interconnect/icc-bcm-voter.ko',
            'qcom-interconnect/qnoc-qos.ko','qcom-interconnect/icc-rpmh.ko','qcom-interconnect/qnoc-alor.ko',
        ]] + [ROOT/'tests/interconnect/avr-interconnect-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            if source.name!='qnoc-alor.ko':
                commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_INTERCONNECT_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += [
            '/bin/busybox rmmod avr_interconnect_test || { echo AVR_INTERCONNECT_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox mkdir -p /sys/kernel/debug',
            '/bin/busybox mount -t debugfs debugfs /sys/kernel/debug || { echo AVR_INTERCONNECT_DEBUGFS_FAIL; /bin/busybox poweroff -f; }',
            'echo 1 > /sys/kernel/debug/interconnect/debug_suspend || { echo AVR_INTERCONNECT_TRACE_ENABLE_FAIL; /bin/busybox poweroff -f; }',
            'echo 0 > /sys/kernel/debug/interconnect/debug_suspend || { echo AVR_INTERCONNECT_TRACE_DISABLE_FAIL; /bin/busybox poweroff -f; }']
        for name in ['icc_rpmh','qnoc_qos','icc_debug']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_INTERCONNECT_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        # Firmware Alor and BCM drivers have no module_exit: test other teardown
        # first, then load the intentionally resident hardware provider.
        for name in ['icc-debug','qnoc-qos','icc-rpmh','qnoc-alor']:
            commands.append(f'/bin/busybox insmod /modules/{name}.ko || {{ echo AVR_INTERCONNECT_RELOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['if /bin/busybox rmmod icc_bcm_voter; then echo AVR_INTERCONNECT_BCM_RESIDENT_GUARD_FAIL; /bin/busybox poweroff -f; fi',
                     'test -d /sys/module/qnoc_alor || { echo AVR_INTERCONNECT_ALOR_STATE_FAIL; /bin/busybox poweroff -f; }',
                     'if /bin/busybox rmmod qnoc_alor; then echo AVR_INTERCONNECT_RESIDENT_GUARD_FAIL; /bin/busybox poweroff -f; fi',
                     'echo AVR_INTERCONNECT_TESTS_PASS scope=aggregation-and-module-lifecycle-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.regulators_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko'] + [ROOT/'ports'/name for name in [
            'qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko',
            'qcom-minidump/minidump.ko','qcom-ipc-logging/qcom_ipc_logging.ko',
            'qcom-clock-core/crm-v2.ko','qcom-rpmh/cmd-db.ko','qcom-rpmh/qcom_rpmh.ko',
            'qcom-clock-core/proxy-consumer.ko','qcom-regulators/debug-regulator.ko',
            'qcom-regulators/rpmh-regulator.ko','qcom-regulators/stub-regulator.ko',
        ]] + [ROOT/'tests/regulators/avr-regulators-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_REGULATORS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += [
            '/bin/busybox mkdir -p /sys/kernel/debug',
            '/bin/busybox mount -t debugfs debugfs /sys/kernel/debug || { echo AVR_REGULATORS_DEBUGFS_FAIL; /bin/busybox poweroff -f; }',
            'rail=/sys/kernel/debug/regulator/avr_qemu_rail',
            'test -f "$rail/enable" || { echo AVR_REGULATORS_FILE_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox cat "$rail/enable" || { echo AVR_REGULATORS_READ_FAIL; /bin/busybox poweroff -f; }',
            'exec 9<"$rail/voltage"',
            '/bin/busybox rmmod avr_regulators_test || { echo AVR_REGULATORS_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            'echo avr-qemu-regulator > /sys/bus/platform/drivers/stub-regulator/unbind || { echo AVR_REGULATORS_UNBIND_FAIL; /bin/busybox poweroff -f; }',
            'if /bin/busybox cat <&9; then echo AVR_REGULATORS_STALE_READ_FAIL; /bin/busybox poweroff -f; fi',
            'exec 9<&-',
            'echo avr-qemu-regulator > /sys/bus/platform/drivers/stub-regulator/bind || { echo AVR_REGULATORS_REBIND_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox cat "$rail/enable" || { echo AVR_REGULATORS_REBIND_READ_FAIL; /bin/busybox poweroff -f; }',
            'if /bin/busybox rmmod stub_regulator; then echo AVR_REGULATORS_REFCOUNT_FAIL; /bin/busybox poweroff -f; fi',
            'echo avr-qemu-regulator > /sys/bus/platform/drivers/stub-regulator/unbind || { echo AVR_REGULATORS_FINAL_UNBIND_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox rmmod stub_regulator || { echo AVR_REGULATORS_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox rmmod rpmh_regulator || { echo AVR_REGULATORS_RPMH_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox rmmod debug_regulator || { echo AVR_REGULATORS_DEBUG_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            'echo AVR_REGULATORS_TESTS_PASS scope=logical-rail-and-teardown-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.diagnostics_tests or args.storage_host_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',
                 ROOT/'ports/qcom-hwspinlock/qcom_hwspinlock.ko',
                 ROOT/'ports/qcom-smem/smem.ko',
                 ROOT/'ports/qcom-minidump/minidump.ko',
                 ROOT/'ports/qcom-ipc-logging/qcom_ipc_logging.ko',
                 ROOT/'tests/diagnostics/avr-diagnostics-test.ko']
        if args.storage_host_tests:
            sources = sources[:-1] + firmware_modules() + [ROOT/'ports'/name for name in [
                'qcom-clock-core/proxy-consumer.ko','qcom-clock-core/gdsc-regulator.ko',
                'qcom-clock-core/crm-v2.ko','qcom-clock-core/clk-qcom.ko',
                'ufs-phy-canoe/phy-qcom-ufs.ko','ufs-phy-canoe/phy-qcom-ufs-qmp-v4-canoe.ko',
                'qcom-ice/qcom_ice.ko','ufs-host/ufshcd-crypto-qti.ko','ufs-host/ufs-qcom.ko',
            ]] + sources[-1:]
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_DIAGNOSTICS_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        # IPC logging intentionally has no module_exit in the firmware source.
        # Its permanent context-list region must remain valid until shutdown.
        commands += ['/bin/busybox rmmod avr_diagnostics_test || { echo AVR_DIAGNOSTICS_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     'echo AVR_DIAGNOSTICS_TESTS_PASS']
        if args.storage_host_tests:
            commands += [
                'test -d /sys/module/ufs_qcom || { echo AVR_STORAGE_HOST_STATE_FAIL; /bin/busybox poweroff -f; }',
                '/bin/busybox rmmod ufs_qcom || { echo AVR_STORAGE_HOST_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                '/bin/busybox rmmod ufshcd_crypto_qti || { echo AVR_STORAGE_CRYPTO_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                'echo AVR_STORAGE_HOST_TESTS_PASS scope=module-lifecycle-only']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.debug_symbol_tests:
        sources=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/debug-symbol/avr-debug-symbol-test.ko']
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_DEBUG_SYMBOL_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['/bin/busybox rmmod avr_debug_symbol_test || { echo AVR_DEBUG_SYMBOL_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     'echo AVR_DEBUG_SYMBOL_TESTS_PASS']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.ufs_tests:
        for source in [ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/ufs/avr-ufs-test.ko']:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
        add('bin/ufs-io-test',stat.S_IFREG|0o755,(ROOT/'tests/ufs/ufs-io-test').read_bytes())
        commands=[
            'tries=0; while test ! -e /sys/block/sda/dev; do tries=$((tries+1)); if test "$tries" -gt 100; then echo AVR_UFS_ENUM_FAIL; /bin/busybox poweroff -f; fi; /bin/busybox sleep 0.1; done',
            '/bin/busybox cat /sys/block/sda/device/vendor /sys/block/sda/device/model',
            'devno=$(/bin/busybox cat /sys/block/sda/dev)',
            '/bin/busybox mknod /dev/avr-ufs b ${devno%:*} ${devno#*:} || { echo AVR_UFS_NODE_FAIL; /bin/busybox poweroff -f; }',
            '/bin/ufs-io-test || { echo AVR_UFS_IO_FAIL; /bin/busybox poweroff -f; }',
            '/bin/busybox insmod /modules/kunit.ko enable=1 || { echo AVR_UFS_KUNIT_LOAD_FAIL; /bin/busybox poweroff -f; }',
            f'/bin/busybox insmod /modules/avr-ufs-test.ko expected_mcq={int(args.ufs_mcq)} || {{ echo AVR_UFS_TEST_LOAD_FAIL; /bin/busybox poweroff -f; }}',
            '/bin/busybox rmmod avr_ufs_test || { echo AVR_UFS_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
            'echo AVR_UFS_TESTS_PASS']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.ice_tests:
        modules=[ROOT/'out/native-gki/lib/kunit/kunit.ko']
        modules += firmware_modules()
        modules += [ROOT/'ports/qcom-ice/qcom_ice.ko',ROOT/'tests/ice/avr-ice-test.ko']
        commands=[]
        for source in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_ICE_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands += ['test -d /sys/module/qcom_ice || { echo AVR_ICE_STATE_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod avr_ice_test || { echo AVR_ICE_TEST_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     '/bin/busybox rmmod qcom_ice || { echo AVR_ICE_UNLOAD_FAIL; /bin/busybox poweroff -f; }',
                     'test ! -d /sys/module/qcom_ice || { echo AVR_ICE_EXIT_STATE_FAIL; /bin/busybox poweroff -f; }',
                     'echo AVR_ICE_TESTS_PASS']
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.storage_phy_tests:
        commands=[]
        for name in ['phy-qcom-ufs','phy-qcom-ufs-qmp-v4-canoe']:
            source=ROOT/'ports/ufs-phy-canoe'/(name+'.ko')
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            module=name.replace('-','_')
            commands += [f'/bin/busybox insmod /modules/{source.name} || {{ echo AVR_STORAGE_PHY_LOAD_FAIL; /bin/busybox poweroff -f; }}',
                         f'test -d /sys/module/{module} || {{ echo AVR_STORAGE_PHY_STATE_FAIL; /bin/busybox poweroff -f; }}']
        commands.append('echo AVR_STORAGE_PHY_LOADED count=2')
        for name in ['phy_qcom_ufs_qmp_v4_canoe','phy_qcom_ufs']:
            commands += [f'/bin/busybox rmmod {name} || {{ echo AVR_STORAGE_PHY_UNLOAD_FAIL; /bin/busybox poweroff -f; }}',
                         f'test ! -d /sys/module/{name} || {{ echo AVR_STORAGE_PHY_EXIT_STATE_FAIL; /bin/busybox poweroff -f; }}']
        commands.append('echo AVR_STORAGE_PHY_TESTS_PASS count=2')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.platform_tests:
        modules=[ROOT/'out/native-gki/lib/kunit/kunit.ko']
        modules += [ROOT/'ports'/name for name in [
            'qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko',
            'qcom-glink-transport/rpmsg_ctrl.ko','qcom-glink-transport/qcom_glink.ko',
                'qcom-glink-transport/qcom_glink_smem.ko','qcom-rproc-common/qcom_common.ko','gunyah/gh_arm_drv.ko',
            'gunyah/gh_dbl.ko','gunyah/gh_msgq.ko',
            'gunyah-core/gunyah_hypercall.ko','gunyah-core/gunyah_platform_hooks.ko']]
        modules += [ROOT/'out/native-gki'/name for name in ['drivers/hwtracing/coresight/coresight.ko','drivers/nvmem/nvmem_qfprom.ko']]
        modules += [ROOT/'tests/remoteproc'/name for name in ['avr-rproc-test.ko','avr-gunyah-test.ko']]
        commands=[]
        for source in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_PLATFORM_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        add('modules/gunyah.ko',stat.S_IFREG|0o644,(ROOT/'ports/gunyah-core/gunyah.ko').read_bytes())
        commands += [
            'if /bin/busybox insmod /modules/gunyah.ko > /tmp/gunyah-gate 2>&1; then echo AVR_GUNYAH_GATE_FAIL; /bin/busybox poweroff -f; fi',
            '/bin/busybox cat /tmp/gunyah-gate',
            '/bin/busybox grep -q "No such device" /tmp/gunyah-gate || { echo AVR_GUNYAH_GATE_ERRNO_FAIL; /bin/busybox poweroff -f; }',
            'test ! -d /sys/module/gunyah || { echo AVR_GUNYAH_GATE_STATE_FAIL; /bin/busybox poweroff -f; }',
            'echo AVR_GUNYAH_GUEST_GATE_PASS']
        for name in ['avr_gunyah_test','avr_rproc_test','nvmem_qfprom','coresight','gh_msgq','gh_dbl','gh_arm_drv','gunyah_platform_hooks','gunyah_hypercall','qcom_common','qcom_glink_smem','qcom_glink','rpmsg_ctrl','smem','qcom_hwspinlock']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_PLATFORM_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_PLATFORM_TESTS_PASS')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.scm_tests:
        modules=[ROOT/'out/native-gki/lib/kunit/kunit.ko']
        modules += firmware_modules()
        modules.append(ROOT/'tests/scm/avr-scm-notifier-test.ko')
        commands=[]
        for source in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_SCM_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_scm_notifier_test || { echo AVR_SCM_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_SCM_TESTS_PASS')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.gpu_policy_tests:
        modules=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/gpu-policy/avr-adrenoboost-test.ko']
        commands=[]
        for source in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_GPU_POLICY_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_GPU_POLICY_TESTS_PASS')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    if args.rm_lifetime_tests:
        modules=[ROOT/'out/native-gki/lib/kunit/kunit.ko',ROOT/'tests/rm-lifetime/avr-rm-notifiers-test.ko']
        commands=[]
        for source in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_RM_LIFETIME_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('/bin/busybox rmmod avr_rm_notifiers_test || { echo AVR_RM_LIFETIME_UNLOAD_FAIL; /bin/busybox poweroff -f; }')
        commands.append('echo AVR_RM_LIFETIME_TESTS_PASS')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    add('init',stat.S_IFREG|0o755,init)
    add('TRAILER!!!',0)
    target=ROOT/'tests/qemu'/('soc-misc-tests.cpio.gz' if args.soc_misc_tests else 'media-data-tests.cpio.gz' if args.media_data_tests else 'audio-conn-tests.cpio.gz' if args.audio_conn_tests else 'msm-drm-tests.cpio.gz' if args.msm_drm_tests else 'mm-drivers-tests.cpio.gz' if args.mm_drivers_tests else 'intree-misc-tests.cpio.gz' if args.intree_misc_tests else 'qcom-charging-tests.cpio.gz' if args.qcom_charging_tests else 'qcom-usb-tests.cpio.gz' if args.qcom_usb_tests else 'qcom-pm-tests.cpio.gz' if args.qcom_pm_tests else 'qcom-perf-misc-tests.cpio.gz' if args.qcom_perf_misc_tests else 'qcom-debug-diag-tests.cpio.gz' if args.qcom_debug_diag_tests else 'firststage-misc-tests.cpio.gz' if args.firststage_misc_tests else 'qcom-cpucp-scmi-tests.cpio.gz' if args.qcom_cpucp_scmi_tests else 'qcom-reboot-tests.cpio.gz' if args.qcom_reboot_tests else 'qcom-tee-tests.cpio.gz' if args.qcom_tee_tests else 'dma-heaps-tests.cpio.gz' if args.dma_heaps_tests else 'moto-sched-tests.cpio.gz' if args.moto_sched_tests else 'thermal-pause-tests.cpio.gz' if args.thermal_pause_tests else 'sched-walt-tests.cpio.gz' if args.sched_walt_tests else 'walt-hooks-tests.cpio.gz' if args.walt_hooks_tests else 'task-vendor-data-tests.cpio.gz' if args.task_vendor_data_tests else 'userspace-cdev-tests.cpio.gz' if args.userspace_cdev_tests else 'cpu-hotplug-tests.cpio.gz' if args.cpu_hotplug_tests else 'cpufreq-cooling-tests.cpio.gz' if args.cpufreq_cooling_tests else 'devfreq-cooling-tests.cpio.gz' if args.devfreq_cooling_tests else 'ddr-cooling-tests.cpio.gz' if args.ddr_cooling_tests else 'qmi-cooling-tests.cpio.gz' if args.qmi_cooling_tests else 'bcl-pmic-tests.cpio.gz' if args.bcl_pmic_tests else 'bcl-soc-tests.cpio.gz' if args.bcl_soc_tests else 'watchdog-tests.cpio.gz' if args.watchdog_tests else 'rng-tests.cpio.gz' if args.rng_tests else 'tsens-tests.cpio.gz' if args.tsens_tests else 'geni-setup-tests.cpio.gz' if args.geni_setup_tests else 'geni-pm-tests.cpio.gz' if args.geni_pm_tests else 'geni-map-tests.cpio.gz' if args.geni_map_tests else 'geni-ring-tests.cpio.gz' if args.geni_ring_tests else 'geni-bus-tests.cpio.gz' if args.geni_bus_tests else 'usb-repeater-pmic-tests.cpio.gz' if args.usb_repeater_pmic_tests else 'usb-repeater-tests.cpio.gz' if args.usb_repeater_tests else 'goodix-mmap-tests.cpio.gz' if args.goodix_mmap_tests else 'mmi-info-native-tests.cpio.gz' if args.mmi_info_native_tests else 'mmi-info-tests.cpio.gz' if args.mmi_info_tests else 'mmi-annotate-native-tests.cpio.gz' if args.mmi_annotate_native_tests else 'mmi-annotate-driver-tests.cpio.gz' if args.mmi_annotate_driver_tests else 'moto-touch-of-tests.cpio.gz' if args.moto_touch_of_tests else 'mmi-annotate-tests.cpio.gz' if args.mmi_annotate_tests else 'moto-touch-deps-tests.cpio.gz' if args.moto_touch_deps_tests else 'panel-event-tests.cpio.gz' if args.panel_event_tests else 'power-key-tests.cpio.gz' if args.power_key_tests else 'sdam-tests.cpio.gz' if args.sdam_tests else 'rtc-tests.cpio.gz' if args.rtc_tests else 'pas-driver-critical-'+args.pas_critical+'-tests.cpio.gz' if args.pas_critical else 'pas-driver-tests.cpio.gz' if args.pas_driver_tests else 'pas-images-tests.cpio.gz' if args.pas_images_tests else 'pas-proxy-tests.cpio.gz' if args.pas_proxy_tests else 'mdt-loader-tests.cpio.gz' if args.mdt_loader_tests else 'pas-rails-tests.cpio.gz' if args.pas_rails_tests else 'remoteproc-services-tests.cpio.gz' if args.remoteproc_services_tests else 'smem-state-tests.cpio.gz' if args.smem_state_tests else 'smp2p-tests.cpio.gz' if args.smp2p_tests else 'ipcc-tests.cpio.gz' if args.ipcc_tests else 'glink-transport-tests.cpio.gz' if args.glink_transport_tests else 'rproc-gate-tests.cpio.gz' if args.rproc_gate_tests else 'pmic-glink-tests.cpio.gz' if args.pmic_glink_tests else 'pmic-ipc-tests.cpio.gz' if args.pmic_ipc_tests else 'adc-thermal-tests.cpio.gz' if args.adc_thermal_tests else 'pmic-gpio-tests.cpio.gz' if args.pmic_gpio_tests else 'spmi-tests.cpio.gz' if args.spmi_tests else 'interconnect-tests.cpio.gz' if args.interconnect_tests else 'regulators-tests.cpio.gz' if args.regulators_tests else 'storage-host-tests.cpio.gz' if args.storage_host_tests else 'diagnostics-tests.cpio.gz' if args.diagnostics_tests else 'debug-symbol-tests.cpio.gz' if args.debug_symbol_tests else 'ufs-tests.cpio.gz' if args.ufs_tests else 'ice-tests.cpio.gz' if args.ice_tests else 'storage-phy-tests.cpio.gz' if args.storage_phy_tests else 'rm-lifetime-tests.cpio.gz' if args.rm_lifetime_tests else 'gpu-policy-tests.cpio.gz' if args.gpu_policy_tests else 'scm-tests.cpio.gz' if args.scm_tests else 'platform-tests.cpio.gz' if args.platform_tests else 'adios-tests.cpio.gz' if args.adios_tests else 'power-tests.cpio.gz' if args.power_tests else 'rakkashin-tests.cpio.gz' if args.rakkashin_tests else 'ntsync-tests.cpio.gz' if args.ntsync_tests else 'fence-tests.cpio.gz' if args.fence_tests else 'sysstats-tests.cpio.gz' if args.sysstats_tests else 'secure-tests.cpio.gz' if args.secure_tests else 'clock-tests.cpio.gz' if args.clock_tests else 'oplus-tests.cpio.gz' if args.oplus_tests else 'modules-tests.cpio.gz' if args.modules_tests else 'iommu-tests.cpio.gz' if args.iommu_tests else 'boot-smoke.cpio.gz')
    with target.open('wb') as f:
        with gzip.GzipFile(fileobj=f,mode='wb',mtime=0) as z:z.write(data)
    print(target,len(data),'uncompressed bytes')

if __name__=='__main__':main()
