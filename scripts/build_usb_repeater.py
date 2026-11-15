#!/usr/bin/env python3
"""Build pinned USB repeater/M31 drafts and optional native registry tests; no IO."""
from pathlib import Path
import argparse,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tests',action='store_true');args=p.parse_args()
subprocess.run([sys.executable,str(ROOT/'scripts/audit_usb_repeater_sources.py')],check=True)
for target in ['ports/usb-repeater']+(['tests/usb-repeater'] if args.tests else []):
 cmd=['chroot',str(ROOT.parent/'.andronix/distros/ubuntu/rootfs'),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j2','modules']
 subprocess.run(['su','-c',shlex.join(cmd)],check=True)
