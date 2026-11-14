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
p.add_argument('--test',choices=['pmic-ipc','adc-thermal','pmic-gpio','spmi','interconnect','regulators','storage-host','diagnostics','debug-symbol','ufs','ice','storage-phy','smoke','clock','oplus','modules','iommu','secure','sysstats','fence','rakkashin','ntsync','power','adios','platform','scm','gpu-policy','rm-lifetime'],required=True)
p.add_argument('--cpus',type=int,choices=[1,2,4],default=2)
p.add_argument('--timeout',type=int,default=120)
p.add_argument('--log',type=Path,required=True)
p.add_argument('--clock-count',type=int,default=35)
p.add_argument('--append',default='',help='Additional guest boot args for explicit diagnostic comparisons')
p.add_argument('--ufs-mcq',action='store_true')
a=p.parse_args()
if a.ufs_mcq and a.test!='ufs':p.error('--ufs-mcq requires --test ufs')
if not 10<=a.timeout<=600:p.error('timeout must be 10..600 seconds')
subprocess.run([sys.executable,str(ROOT/'scripts/make_qemu_initramfs.py')]+([] if a.test=='smoke' else ['--'+a.test+'-tests'])+(['--ufs-mcq'] if a.ufs_mcq else []),check=True)
ramdisk={'pmic-ipc':'pmic-ipc-tests','adc-thermal':'adc-thermal-tests','pmic-gpio':'pmic-gpio-tests','spmi':'spmi-tests','interconnect':'interconnect-tests','regulators':'regulators-tests','storage-host':'storage-host-tests','diagnostics':'diagnostics-tests','debug-symbol':'debug-symbol-tests','ufs':'ufs-tests','ice':'ice-tests','storage-phy':'storage-phy-tests','smoke':'boot-smoke','clock':'clock-tests','oplus':'oplus-tests','modules':'modules-tests','iommu':'iommu-tests','secure':'secure-tests','sysstats':'sysstats-tests','fence':'fence-tests','rakkashin':'rakkashin-tests','ntsync':'ntsync-tests','power':'power-tests','rm-lifetime':'rm-lifetime-tests','gpu-policy':'gpu-policy-tests','scm':'scm-tests','platform':'platform-tests','adios':'adios-tests'}[a.test]
command=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','timeout',str(a.timeout),
'qemu-system-aarch64','-machine','virt,gic-version=3','-cpu','max','-smp',str(a.cpus),'-m','1024','-accel','tcg,thread=multi',
'-nodefaults','-serial','stdio','-monitor','none','-display','none','-no-reboot',
'-kernel','/work/avr-port/out/native-gki/arch/arm64/boot/Image','-initrd','/work/avr-port/tests/qemu/'+ramdisk+'.cpio.gz',
'-append','console=ttyAMA0 earlycon=pl011,0x09000000 rdinit=/init panic=1 '+a.append]
image=ROOT/'out/native-gki/arch/arm64/boot/Image'
metadata={'initramfs_sha256':hashlib.sha256((ROOT/'tests/qemu'/(ramdisk+'.cpio.gz')).read_bytes()).hexdigest(),'test':a.test,'cpus':a.cpus,'image_sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'command':command}
disk=None
if a.test=='ufs':
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
if a.test=='regulators':
    folder=ROOT/'artifacts/qemu-regulators'
    folder.mkdir(parents=True,exist_ok=True)
    guest='/work/avr-port/artifacts/qemu-regulators/'
    dump=command.copy()
    dump[dump.index('-machine')+1] += ',dumpdtb='+guest+'virt-original.dtb'
    dtc='/work/avr-port/out/native-gki/scripts/dtc/'
    prefix=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin']
    with (ROOT/'reports/regulators-dtb-build.log').open('wb') as stream:
        for step in [dump,
                     prefix+[dtc+'dtc','-@','-I','dts','-O','dtb','-o',guest+'rail.dtbo','/work/avr-port/tests/regulators/qemu-rail.dtso'],
                     prefix+[dtc+'fdtoverlay','-i',guest+'virt-original.dtb','-o',guest+'virt-rail.dtb',guest+'rail.dtbo']]:
            subprocess.run(['su','-c',shlex.join(step)],stdout=stream,stderr=subprocess.STDOUT,check=True)
    command += ['-dtb',guest+'virt-rail.dtb']
    metadata.update(guest_dtb_sha256=hashlib.sha256((folder/'virt-rail.dtb').read_bytes()).hexdigest(),
                    dt_scope='synthetic QEMU logical rail only; never an AVR device DT')
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
check=subprocess.run([sys.executable,str(ROOT/'scripts/check_qemu_log.py'),str(a.log),'--test',a.test,'--clock-count',str(a.clock_count)])
raise SystemExit(result.returncode or check.returncode or int(extra_failed))
