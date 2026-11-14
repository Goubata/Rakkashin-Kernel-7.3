#!/usr/bin/env python3
"""Run isolated, diskless ARM64 tests. Never loads modules into the phone kernel."""
import argparse
import hashlib
import json
from pathlib import Path
import shlex
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[1]
ROOTFS=ROOT.parent/'.andronix/distros/ubuntu/rootfs'
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--test',choices=['smoke','clock','oplus','modules','iommu','secure','sysstats','fence','rakkashin','ntsync','power','adios','platform'],required=True)
p.add_argument('--cpus',type=int,choices=[1,2,4],default=2)
p.add_argument('--timeout',type=int,default=120)
p.add_argument('--log',type=Path,required=True)
p.add_argument('--clock-count',type=int,default=35)
p.add_argument('--append',default='',help='Additional guest boot args for explicit diagnostic comparisons')
a=p.parse_args()
if not 10<=a.timeout<=600:p.error('timeout must be 10..600 seconds')
subprocess.run([sys.executable,str(ROOT/'scripts/make_qemu_initramfs.py')]+([] if a.test=='smoke' else ['--'+a.test+'-tests']),check=True)
ramdisk={'smoke':'boot-smoke','clock':'clock-tests','oplus':'oplus-tests','modules':'modules-tests','iommu':'iommu-tests','secure':'secure-tests','sysstats':'sysstats-tests','fence':'fence-tests','rakkashin':'rakkashin-tests','ntsync':'ntsync-tests','power':'power-tests','platform':'platform-tests','adios':'adios-tests'}[a.test]
command=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','timeout',str(a.timeout),
'qemu-system-aarch64','-machine','virt,gic-version=3','-cpu','max','-smp',str(a.cpus),'-m','1024','-accel','tcg,thread=multi',
'-nodefaults','-serial','stdio','-monitor','none','-display','none','-no-reboot',
'-kernel','/work/avr-port/out/native-gki/arch/arm64/boot/Image','-initrd','/work/avr-port/tests/qemu/'+ramdisk+'.cpio.gz',
'-append','console=ttyAMA0 earlycon=pl011,0x09000000 rdinit=/init panic=1 '+a.append]
image=ROOT/'out/native-gki/arch/arm64/boot/Image'
metadata={'initramfs_sha256':hashlib.sha256((ROOT/'tests/qemu'/(ramdisk+'.cpio.gz')).read_bytes()).hexdigest(),'test':a.test,'cpus':a.cpus,'image_sha256':hashlib.sha256(image.read_bytes()).hexdigest(),'command':command}
start=time.monotonic()
with a.log.open('wb') as f:
 result=subprocess.run(['su','-c',shlex.join(command)],stdout=f,stderr=subprocess.STDOUT)
metadata.update(returncode=result.returncode,elapsed_seconds=round(time.monotonic()-start,3))
a.log.with_suffix('.json').write_text(json.dumps(metadata,indent=2)+'\n')
check=subprocess.run([sys.executable,str(ROOT/'scripts/check_qemu_log.py'),str(a.log),'--test',a.test,'--clock-count',str(a.clock_count)])
raise SystemExit(result.returncode or check.returncode)
