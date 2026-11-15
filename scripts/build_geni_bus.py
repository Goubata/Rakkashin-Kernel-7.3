#!/usr/bin/env python3
"""Build pinned GENI DMA/I2C/SPI drafts with real dependencies; never phone IO."""
from pathlib import Path
import argparse,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tests',action='store_true');p.add_argument('--ring-tests',action='store_true');a=p.parse_args()
for name in ['audit_core_patch_replay.py','audit_geni_bus_sources.py']:
 subprocess.run([sys.executable,str(ROOT/'scripts'/name)],check=True)
if a.tests:
 s=(ROOT/'ports/geni-bus/spi-msm-geni.c').read_text();start=s.index('static int spi_geni_get_chip_select_num(');end=s.index('\n}',start)+2
 (ROOT/'tests/geni-bus/generated.h').write_text('/* Exact production chip-select helper. */\n'+s[start:end]+'\n')
for target in ['ports/geni-bus']+(['tests/geni-bus'] if a.tests else [])+(['tests/geni-ring'] if a.ring_tests else []):
 command=['chroot',str(ROOT.parent/'.andronix/distros/ubuntu/rootfs'),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j2','modules']
 subprocess.run(['su','-c',shlex.join(command)],check=True)
