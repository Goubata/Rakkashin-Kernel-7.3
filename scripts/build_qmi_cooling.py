#!/usr/bin/env python3
"""Build QMI cooling draft and optional native local-socket tests; no phone IO."""
from pathlib import Path
import argparse,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tests',action='store_true');a=p.parse_args()
for name in ['audit_core_patch_replay.py','audit_qmi_cooling_sources.py','audit_qmi_phone_fixture.py']:
 subprocess.run([sys.executable,str(ROOT/'scripts'/name)],check=True)
for target in ['ports/qcom-qmi-cooling']+(['tests/qmi-cooling'] if a.tests else []):
 command=['chroot',str(ROOT.parent/'.andronix/distros/ubuntu/rootfs'),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j2','modules']
 subprocess.run(['su','-c',shlex.join(command)],check=True)
