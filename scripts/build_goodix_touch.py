#!/usr/bin/env python3
"""Build pinned Goodix/THP and real Motorola prerequisites as drafts; no device IO."""
import argparse
from pathlib import Path
import shlex
import subprocess
import sys
ROOT=Path(__file__).resolve().parents[1]
ROOTFS=ROOT.parent/'.andronix/distros/ubuntu/rootfs'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--annotate-tests',action='store_true');args=p.parse_args()
for name in ['moto_info','goodix_touch']:
    subprocess.run([sys.executable,str(ROOT/'scripts'/('audit_'+name+'_sources.py'))],check=True)
subprocess.run([sys.executable,str(ROOT/'scripts/build_storage_deps.py'),'--diagnostics','--panel-event','--moto-touch-deps'],check=True)
targets=['ports/qcom-hwspinlock','ports/qcom-smem','ports/moto-info','ports/goodix-touch']
if args.annotate_tests:targets.append('tests/mmi-annotate')
for target in targets:
    cmd=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','KBUILD_BUILD_USER=avr-builder','KBUILD_BUILD_HOST=avr-arm64','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j2','modules']
    subprocess.run(['su','-c',shlex.join(cmd)],check=True)
