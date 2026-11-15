#!/usr/bin/env python3
"""Exercise exact production payload functions; no physical touch device."""
from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[2]
src=ROOT/'ports/goodix-touch/goodix_thp/goodix_thp.c'
header=ROOT/'ports/goodix-touch/goodix_thp/goodix_thp.h'
s=src.read_text();h=header.read_text()
def function(name):
 a=s.index(name+'(');a=s.rfind('\n',0,a)+1;b=s.index('\n}',a)+2;return s[a:b]
a=h.index('struct driver_request_app_pkg {');b=h.index('\nstruct thp_ioctl_spi_trans_data',a)
# thp_ioctl_frame is naturally aligned, outside the factory packed region.
structs='#pragma pack(push, 1)\n'+h[a:h.index('#pragma pack(pop)',a)]+'\n#pragma pack(pop)\n'+h[h.index('struct thp_ioctl_frame {',a):b]
a=h.index('enum {\n        SVC_CMD_MMAP_DEQUEUE');b=h.index('\nstruct thp_ioctl_update_info',a);structs+=h[a:b]
functions='\n'.join(function(n) for n in ['put_frame_list','goodix_thp_ioctl_get_frame','goodix_thp_ioctl_recv_tsc_msg'])
fixture=(Path(__file__).parent/'fixture.c').read_text().replace('@STRUCTS@',structs).replace('@FUNCTIONS@',functions)
out=ROOT/'artifacts/goodix-payload-stage59';out.mkdir(parents=True,exist_ok=True);(out/'test.c').write_text(fixture)
subprocess.run(['clang','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-variable','-O1','-ftrivial-auto-var-init=pattern',str(out/'test.c'),'-o',str(out/'test')],check=True)
r=subprocess.run([str(out/'test')],check=True,capture_output=True,text=True)
report={'passed':True,'scope':'exact production frame enqueue/get and TSC message functions with recorded primitives; not mmap ownership, driver lifetime, native uaccess or physical validation','output':r.stdout.strip(),'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(fixture.encode()).hexdigest()}
(ROOT/'reports/goodix-payload-stage59-tests.json').write_text(json.dumps(report,indent=2)+'\n');print(r.stdout,end='')
