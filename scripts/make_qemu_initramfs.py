#!/usr/bin/env python3
"""Create an isolated RAM-only boot smoke test; it cannot access phone disks."""
from pathlib import Path
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
    args=parser.parse_args()
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
        modules += [ROOT/'out/native-gki/drivers/firmware/qcom'/name for name in
                    ['qcom_tzmem.ko','qcom_pas.ko','qcom-scm.ko']]
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
        modules += [(ROOT/'out/native-gki/drivers/firmware/qcom'/name,'') for name in
                    ['qcom_tzmem.ko','qcom_pas.ko','qcom-scm.ko']]
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
    if args.platform_tests:
        modules=[ROOT/'out/native-gki/lib/kunit/kunit.ko']
        modules += [ROOT/'ports'/name for name in [
            'qcom-hwspinlock/qcom_hwspinlock.ko','qcom-smem/smem.ko',
            'qcom-rproc-common/qcom_common.ko','gunyah/gh_arm_drv.ko',
            'gunyah/gh_dbl.ko','gunyah/gh_msgq.ko']]
        modules += [ROOT/'tests/remoteproc'/name for name in ['avr-rproc-test.ko','avr-gunyah-test.ko']]
        commands=[]
        for source in modules:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{source.name}{params} || {{ echo AVR_PLATFORM_LOAD_FAIL; /bin/busybox poweroff -f; }}')
        for name in ['avr_gunyah_test','avr_rproc_test','gh_msgq','gh_dbl','gh_arm_drv','qcom_common','smem','qcom_hwspinlock']:
            commands.append(f'/bin/busybox rmmod {name} || {{ echo AVR_PLATFORM_UNLOAD_FAIL; /bin/busybox poweroff -f; }}')
        commands.append('echo AVR_PLATFORM_TESTS_PASS')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\n'.join(commands).encode())
    add('init',stat.S_IFREG|0o755,init)
    add('TRAILER!!!',0)
    target=ROOT/'tests/qemu'/('platform-tests.cpio.gz' if args.platform_tests else 'adios-tests.cpio.gz' if args.adios_tests else 'power-tests.cpio.gz' if args.power_tests else 'rakkashin-tests.cpio.gz' if args.rakkashin_tests else 'ntsync-tests.cpio.gz' if args.ntsync_tests else 'fence-tests.cpio.gz' if args.fence_tests else 'sysstats-tests.cpio.gz' if args.sysstats_tests else 'secure-tests.cpio.gz' if args.secure_tests else 'clock-tests.cpio.gz' if args.clock_tests else 'oplus-tests.cpio.gz' if args.oplus_tests else 'modules-tests.cpio.gz' if args.modules_tests else 'iommu-tests.cpio.gz' if args.iommu_tests else 'boot-smoke.cpio.gz')
    with target.open('wb') as f:
        with gzip.GzipFile(fileobj=f,mode='wb',mtime=0) as z:z.write(data)
    print(target,len(data),'uncompressed bytes')

if __name__=='__main__':main()
