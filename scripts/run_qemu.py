#!/usr/bin/env python3
"""Run isolated ARM64 tests; optional UFS uses private regular files, never phone storage."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import struct
import time

ROOT=Path(__file__).resolve().parents[1]
ROOTFS=ROOT.parent/'.andronix/distros/ubuntu/rootfs'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--test',choices=['firststage-misc','qcom-cpucp-scmi','qcom-reboot','qcom-tee','dma-heaps','moto-sched','thermal-pause','sched-walt','walt-hooks','task-vendor-data','userspace-cdev','cpu-hotplug','cpufreq-cooling','devfreq-cooling','ddr-cooling','qmi-cooling','bcl-pmic','bcl-soc','watchdog','rng','tsens','geni-setup','geni-pm','geni-map','geni-ring','geni-bus','usb-repeater-pmic','usb-repeater','goodix-mmap','mmi-info-native','mmi-info','mmi-annotate-native','mmi-annotate-driver','moto-touch-of','mmi-annotate','moto-touch-deps','panel-event','power-key','sdam','rtc','pas-driver','pas-images','pas-proxy','mdt-loader','pas-rails','remoteproc-services','smem-state','smp2p','ipcc','glink-transport','rproc-gate','pmic-glink','pmic-ipc','adc-thermal','pmic-gpio','spmi','interconnect','regulators','storage-host','diagnostics','debug-symbol','ufs','ice','storage-phy','smoke','clock','oplus','modules','iommu','secure','sysstats','fence','rakkashin','ntsync','power','adios','platform','scm','gpu-policy','rm-lifetime'],required=True)
p.add_argument('--pas-critical',choices=['auth','shutdown'],help='Expected fatal path in isolated PAS test guest only')
p.add_argument('--cpus',type=int,choices=[1,2,4],default=2)
p.add_argument('--timeout',type=int,default=120)
p.add_argument('--log',type=Path,required=True)
p.add_argument('--clock-count',type=int,default=35)
p.add_argument('--append',default='',help='Additional guest boot args for explicit diagnostic comparisons')
p.add_argument('--ufs-mcq',action='store_true')
a=p.parse_args()
if a.pas_critical and a.test!='pas-driver':p.error('--pas-critical requires --test pas-driver')
if a.ufs_mcq and a.test!='ufs':p.error('--ufs-mcq requires --test ufs')
if not 10<=a.timeout<=600:p.error('timeout must be 10..600 seconds')
subprocess.run([sys.executable,str(ROOT/'scripts/make_qemu_initramfs.py')]+([] if a.test=='smoke' else ['--'+a.test+'-tests'])+(['--ufs-mcq'] if a.ufs_mcq else [])+(['--pas-critical',a.pas_critical] if a.pas_critical else []),check=True)
ramdisk={'firststage-misc':'firststage-misc-tests','qcom-cpucp-scmi':'qcom-cpucp-scmi-tests','qcom-reboot':'qcom-reboot-tests','qcom-tee':'qcom-tee-tests','dma-heaps':'dma-heaps-tests','moto-sched':'moto-sched-tests','thermal-pause':'thermal-pause-tests','sched-walt':'sched-walt-tests','walt-hooks':'walt-hooks-tests','task-vendor-data':'task-vendor-data-tests','userspace-cdev':'userspace-cdev-tests','cpu-hotplug':'cpu-hotplug-tests','cpufreq-cooling':'cpufreq-cooling-tests','devfreq-cooling':'devfreq-cooling-tests','ddr-cooling':'ddr-cooling-tests','qmi-cooling':'qmi-cooling-tests','bcl-pmic':'bcl-pmic-tests','bcl-soc':'bcl-soc-tests','watchdog':'watchdog-tests','rng':'rng-tests','tsens':'tsens-tests','geni-setup':'geni-setup-tests','geni-pm':'geni-pm-tests','geni-map':'geni-map-tests','geni-ring':'geni-ring-tests','geni-bus':'geni-bus-tests','usb-repeater-pmic':'usb-repeater-pmic-tests','usb-repeater':'usb-repeater-tests','goodix-mmap':'goodix-mmap-tests','mmi-info-native':'mmi-info-native-tests','mmi-info':'mmi-info-tests','mmi-annotate-native':'mmi-annotate-native-tests','mmi-annotate-driver':'mmi-annotate-driver-tests','moto-touch-of':'moto-touch-of-tests','mmi-annotate':'mmi-annotate-tests','moto-touch-deps':'moto-touch-deps-tests','panel-event':'panel-event-tests','power-key':'power-key-tests','sdam':'sdam-tests','rtc':'rtc-tests','pas-driver':'pas-driver-tests','pas-images':'pas-images-tests','pas-proxy':'pas-proxy-tests','mdt-loader':'mdt-loader-tests','pas-rails':'pas-rails-tests','remoteproc-services':'remoteproc-services-tests','smem-state':'smem-state-tests','smp2p':'smp2p-tests','ipcc':'ipcc-tests','glink-transport':'glink-transport-tests','rproc-gate':'rproc-gate-tests','pmic-glink':'pmic-glink-tests','pmic-ipc':'pmic-ipc-tests','adc-thermal':'adc-thermal-tests','pmic-gpio':'pmic-gpio-tests','spmi':'spmi-tests','interconnect':'interconnect-tests','regulators':'regulators-tests','storage-host':'storage-host-tests','diagnostics':'diagnostics-tests','debug-symbol':'debug-symbol-tests','ufs':'ufs-tests','ice':'ice-tests','storage-phy':'storage-phy-tests','smoke':'boot-smoke','clock':'clock-tests','oplus':'oplus-tests','modules':'modules-tests','iommu':'iommu-tests','secure':'secure-tests','sysstats':'sysstats-tests','fence':'fence-tests','rakkashin':'rakkashin-tests','ntsync':'ntsync-tests','power':'power-tests','rm-lifetime':'rm-lifetime-tests','gpu-policy':'gpu-policy-tests','scm':'scm-tests','platform':'platform-tests','adios':'adios-tests'}[a.test]
if a.pas_critical:ramdisk='pas-driver-critical-'+a.pas_critical+'-tests'
command=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','timeout',str(a.timeout),
'qemu-system-aarch64','-machine','virt,gic-version=3','-cpu','max','-smp',str(a.cpus)+(',sockets=1,clusters=2,cores=2,threads=1' if a.test in ('sched-walt','thermal-pause','moto-sched','qcom-cpucp-scmi') else ''),'-m','1024','-accel','tcg,thread=multi',
'-nodefaults','-serial','stdio','-monitor','none','-display','none','-no-reboot',
'-kernel','/work/avr-port/out/native-gki/arch/arm64/boot/Image','-initrd','/work/avr-port/tests/qemu/'+ramdisk+'.cpio.gz',
'-append','console=ttyAMA0 earlycon=pl011,0x09000000 rdinit=/init panic=1 '+('android_arch_task_struct_size=512 ' if a.test in ('sched-walt','thermal-pause','moto-sched') else '')+a.append]
image=ROOT/'out/native-gki/arch/arm64/boot/Image'
metadata={'initramfs_sha256':hashlib.sha256((ROOT/'tests/qemu'/(ramdisk+'.cpio.gz')).read_bytes()).hexdigest(),'test':a.test,'pas_critical':a.pas_critical,'cpus':a.cpus,'image_sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'command':command}
disk=None
if a.test in ('ufs','firststage-misc'):
    folder=ROOT/'artifacts/qemu-ufs'
    folder.mkdir(parents=True,exist_ok=True)
    disk=folder/(a.log.stem+'-'+str(time.monotonic_ns())+'.raw')
    with disk.open('xb') as stream:stream.truncate(64*1024*1024)
    guest='/work/avr-port/'+disk.relative_to(ROOT).as_posix()
    command += ['-drive','if=none,id=avrufsdata,format=raw,file='+guest,
                '-device','ufs,id=avrufs,mcq='+('on' if a.ufs_mcq else 'off')+',mcq-maxq=4',
                '-device','ufs-lu,drive=avrufsdata,lun=0']
    metadata.update(ufs_mcq=a.ufs_mcq,virtual_backing_file=str(disk.relative_to(ROOT)),
                    backing_scope='new private regular file; no phone block device exposed')
if a.test in ('qcom-cpucp-scmi','qcom-reboot','qcom-tee','dma-heaps','moto-sched','thermal-pause','sched-walt','userspace-cdev','cpu-hotplug','cpufreq-cooling','devfreq-cooling','ddr-cooling','qmi-cooling','bcl-pmic','bcl-soc','geni-bus','usb-repeater','regulators','smem-state','remoteproc-services','moto-touch-of','mmi-annotate-driver','mmi-annotate-native','mmi-info','mmi-info-native'):
    folder=ROOT/'artifacts'/('qemu-'+a.test)
    folder.mkdir(parents=True,exist_ok=True)
    guest='/work/avr-port/artifacts/qemu-'+a.test+'/'
    dump=command.copy()
    dump[dump.index('-machine')+1] += ',dumpdtb='+guest+'virt-original.dtb'
    dtc='/work/avr-port/out/native-gki/scripts/dtc/'
    prefix=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin']
    overlay='/work/avr-port/tests/'+('qcom-cpucp-scmi/qemu-scmi.dtso' if a.test=='qcom-cpucp-scmi' else 'qcom-reboot/qemu-reboot.dtso' if a.test=='qcom-reboot' else 'qcom-tee/qemu-tee.dtso' if a.test=='qcom-tee' else 'dma-heaps/qemu-heaps.dtso' if a.test=='dma-heaps' else 'sched-walt/qemu-walt.dtso' if a.test=='moto-sched' else 'thermal-pause/qemu-pause.dtso' if a.test=='thermal-pause' else 'sched-walt/qemu-walt.dtso' if a.test=='sched-walt' else 'userspace-cdev/qemu-userspace.dtso' if a.test=='userspace-cdev' else 'cpu-hotplug/qemu-hotplug.dtso' if a.test=='cpu-hotplug' else 'cpufreq-cooling/qemu-cpufreq.dtso' if a.test=='cpufreq-cooling' else 'devfreq-cooling/qemu-gpu.dtso' if a.test=='devfreq-cooling' else 'ddr-cooling/qemu-ddr.dtso' if a.test=='ddr-cooling' else 'qmi-cooling/qemu-qmi.dtso' if a.test=='qmi-cooling' else 'bcl-pmic/qemu-probe.dtso' if a.test=='bcl-pmic' else 'bcl-soc/qemu-bcl.dtso' if a.test=='bcl-soc' else 'geni-bus/qemu-geni.dtso' if a.test=='geni-bus' else 'usb-repeater/qemu-repeater.dtso' if a.test=='usb-repeater' else 'mmi-info/qemu-info.dtso' if a.test in ('mmi-info','mmi-info-native') else 'mmi-annotate-native/qemu-annotate.dtso' if a.test=='mmi-annotate-native' else 'mmi-annotate-driver/qemu-annotate.dtso' if a.test=='mmi-annotate-driver' else 'moto-touch-of/qemu-relay.dtso' if a.test=='moto-touch-of' else 'regulators/qemu-rail.dtso' if a.test=='regulators' else 'remoteproc-services/qemu-services.dtso' if a.test=='remoteproc-services' else 'smem-state/qemu-state.dtso')
    with (ROOT/'reports'/(a.test+'-dtb-build.log')).open('wb') as stream:
        for step in [dump,
                     prefix+[dtc+'dtc','-@','-I','dts','-O','dtb','-o',guest+'rail.dtbo',overlay],
                     prefix+[dtc+'fdtoverlay','-i',guest+'virt-original.dtb','-o',guest+'virt-rail.dtb',guest+'rail.dtbo']]:
            subprocess.run(['su','-c',shlex.join(step)],stdout=stream,stderr=subprocess.STDOUT,check=True)
    command += ['-dtb',guest+'virt-rail.dtb']
    metadata.update(guest_dtb_sha256=hashlib.sha256((folder/'virt-rail.dtb').read_bytes()).hexdigest(),
                    dt_scope='synthetic QEMU '+a.test+' nodes only; never an AVR device DT')
start=time.monotonic()
with a.log.open('wb') as f:
 result=subprocess.run(['su','-c',shlex.join(command)],stdout=f,stderr=subprocess.STDOUT)
metadata.update(returncode=result.returncode,elapsed_seconds=round(time.monotonic()-start,3))
extra_failed=False
if disk is not None:
    with disk.open('rb') as stream:
        for off in range(0,4*1024*1024,65536):
            raw=stream.read(65536)
            expected=b''.join(struct.pack('<Q',(((off+i)//8)*0x9e3779b97f4a7c15 ^ (2*0xd1b54a32d192ed03)) & ((1<<64)-1)) for i in range(0,65536,8))
            if raw!=expected:
                extra_failed=True
                break
    metadata['host_backing_pattern_matches']=not extra_failed
    metadata['host_backing_bytes_verified']=4*1024*1024 if not extra_failed else off
a.log.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n')
check=subprocess.run([sys.executable,str(ROOT/'scripts/check_qemu_log.py'),str(a.log),'--test',a.test,'--clock-count',str(a.clock_count)]+(['--pas-critical',a.pas_critical] if a.pas_critical else []))
raise SystemExit(result.returncode or check.returncode or int(extra_failed))
