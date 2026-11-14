#!/usr/bin/env python3
"""Create an isolated RAM-only boot smoke test; it cannot access phone disks."""
from pathlib import Path
from native_module_inventory import firmware_modules
import gzip
import argparse
import stat

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
    parser.add_argument("--ufs-mcq",action="store_true")
    args=parser.parse_args()
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
            'qcom-rproc-common/qcom_common.ko','gunyah/gh_arm_drv.ko',
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
        for name in ['avr_gunyah_test','avr_rproc_test','nvmem_qfprom','coresight','gh_msgq','gh_dbl','gh_arm_drv','gunyah_platform_hooks','gunyah_hypercall','qcom_common','smem','qcom_hwspinlock']:
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
    target=ROOT/'tests/qemu'/('interconnect-tests.cpio.gz' if args.interconnect_tests else 'regulators-tests.cpio.gz' if args.regulators_tests else 'storage-host-tests.cpio.gz' if args.storage_host_tests else 'diagnostics-tests.cpio.gz' if args.diagnostics_tests else 'debug-symbol-tests.cpio.gz' if args.debug_symbol_tests else 'ufs-tests.cpio.gz' if args.ufs_tests else 'ice-tests.cpio.gz' if args.ice_tests else 'storage-phy-tests.cpio.gz' if args.storage_phy_tests else 'rm-lifetime-tests.cpio.gz' if args.rm_lifetime_tests else 'gpu-policy-tests.cpio.gz' if args.gpu_policy_tests else 'scm-tests.cpio.gz' if args.scm_tests else 'platform-tests.cpio.gz' if args.platform_tests else 'adios-tests.cpio.gz' if args.adios_tests else 'power-tests.cpio.gz' if args.power_tests else 'rakkashin-tests.cpio.gz' if args.rakkashin_tests else 'ntsync-tests.cpio.gz' if args.ntsync_tests else 'fence-tests.cpio.gz' if args.fence_tests else 'sysstats-tests.cpio.gz' if args.sysstats_tests else 'secure-tests.cpio.gz' if args.secure_tests else 'clock-tests.cpio.gz' if args.clock_tests else 'oplus-tests.cpio.gz' if args.oplus_tests else 'modules-tests.cpio.gz' if args.modules_tests else 'iommu-tests.cpio.gz' if args.iommu_tests else 'boot-smoke.cpio.gz')
    with target.open('wb') as f:
        with gzip.GzipFile(fileobj=f,mode='wb',mtime=0) as z:z.write(data)
    print(target,len(data),'uncompressed bytes')

if __name__=='__main__':main()
