#!/usr/bin/env python3
"""Build pinned GENI DMA/I2C/SPI drafts with real dependencies; never phone IO."""
from pathlib import Path
import argparse,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tests',action='store_true');p.add_argument('--ring-tests',action='store_true');p.add_argument('--map-tests',action='store_true');a=p.parse_args()
for name in ['audit_core_patch_replay.py','audit_geni_bus_sources.py']:
 subprocess.run([sys.executable,str(ROOT/'scripts'/name)],check=True)
if a.tests:
 s=(ROOT/'ports/geni-bus/spi-msm-geni.c').read_text();start=s.index('static int spi_geni_get_chip_select_num(');end=s.index('\n}',start)+2
 clock_start=s.index('static int get_spi_clk_cfg(');clock_end=s.index('\n}',clock_start)+2
 (ROOT/'tests/geni-bus/generated.h').write_text('/* Exact production SPI helpers. */\n'+s[start:end]+'\n'+s[clock_start:clock_end]+'\n')
if a.map_tests:
 s=(ROOT/'ports/geni-bus/spi-msm-geni.c').read_text()
 methods=[]
 for name in ['static void spi_geni_unmap_buf(', 'static int spi_geni_map_buf(']:
  start=s.index(name);end=s.index('\n}',start)+2;methods.append(s[start:end])
 (ROOT/'tests/geni-map/generated.h').write_text('/* Exact production SPI map/unmap. */\n'+'\n'.join(methods)+'\n')
for target in ['ports/geni-bus']+(['tests/geni-bus'] if a.tests else [])+(['tests/geni-ring'] if a.ring_tests else [])+(['tests/geni-map'] if a.map_tests else []):
 command=['chroot',str(ROOT.parent/'.andronix/distros/ubuntu/rootfs'),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j2','modules']
 subprocess.run(['su','-c',shlex.join(command)],check=True)
