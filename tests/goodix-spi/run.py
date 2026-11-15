#!/usr/bin/env python3
"""Compile and run the exact production multi-SPI method with recorded primitives."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
ROOT=Path(__file__).resolve().parents[2]
src=ROOT/'ports/goodix-touch/goodix_thp/goodix_thp.c'
header=ROOT/'ports/goodix-touch/goodix_thp/goodix_thp.h'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--stage',type=int,default=61);args=p.parse_args()
s=src.read_text();start=s.index('static long goodix_thp_ioctl_multi_spi_trans(');end=s.index('\nstatic long ',start+1)
h=header.read_text();a=h.index('struct thp_ioctl_spi_trans_data {');b=h.index('\nenum {',a)
single_start=s.index('static long goodix_thp_ioctl_spi_trans(');single_end=s.index('\nstatic long ',single_start+1)
fixture=(Path(__file__).parent/'fixture.c').read_text().replace('@FUNCTION@',s[single_start:single_end]+'\n'+s[start:end]).replace('@STRUCTS@',h[a:b])
out=ROOT/('artifacts/goodix-spi-stage'+str(args.stage));out.mkdir(parents=True,exist_ok=True)
(out/'test.c').write_text(fixture)
subprocess.run(['clang','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-variable','-O1','-g',str(out/'test.c'),'-o',str(out/'test')],check=True)
r=subprocess.run([str(out/'test')],check=True,text=True,capture_output=True)
report={'scope':'exact production single/multi-SPI functions with recorded allocation/uaccess/SPI primitives; no hardware or full driver lifetime proof','passed':True,'output':r.stdout.strip(),'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(fixture.encode()).hexdigest()}
(ROOT/('reports/goodix-spi-stage'+str(args.stage)+'-tests.json')).write_text(json.dumps(report,indent=2)+'\n')
print(r.stdout,end='')
