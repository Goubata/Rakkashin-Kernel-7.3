#!/usr/bin/env python3
"""Compile exact wire read/write/frame methods with guarded private buffers."""
from pathlib import Path
import hashlib,json,re,subprocess
ROOT=Path(__file__).resolve().parents[2];src=ROOT/'ports/goodix-touch/goodix_thp/goodix_thp_spi.c';s=src.read_text();h=(src.parent/'goodix_thp.h').read_text()
def extract(name):
 a=s.index(name+'(');a=s.rfind('\n',0,a)+1;b=s.index('\n}',a)+2;return s[a:b]
names=['GOODIX_THP_MAX_FRAME_LEN','GOODIX_SPI_BUFF_MAX_SIZE','FRAME_HEAD_LEN','CHIP_TYPE_9897','CHIP_TYPE_9916','SPI_FLAG_RD','SPI_FLAG_WR','MOVE_24BIT','MOVE_16BIT','MOVE_8BIT','MASK_8BIT','GOODIX_READ_WRITE_BYTE_OFFSET_GT9897','GOODIX_READ_WRITE_BYTE_OFFSET_GT9916']
constants='\n'.join(re.search(r'^#define '+n+r'\s+[^\n]+',s+'\n'+h,re.M).group(0) for n in names)
fixture=(Path(__file__).parent/'fixture.c').read_text().replace('@CONSTANTS@',constants).replace('@FUNCTIONS@','\n'.join(extract(n) for n in ['goodix_thp_spi_read','goodix_thp_spi_write','goodix_thp_get_frame']))
out=ROOT/'artifacts/goodix-wire-stage63';out.mkdir(parents=True,exist_ok=True);(out/'test.c').write_text(fixture)
subprocess.run(['clang','-std=gnu11','-Wall','-Wextra','-Werror','-Wno-pointer-sign','-O1','-ftrivial-auto-var-init=pattern',str(out/'test.c'),'-o',str(out/'test')],check=True)
r=subprocess.run([str(out/'test')],check=True,capture_output=True,text=True)
report={'passed':True,'scope':'Exact read/write/get-frame source methods with recorded SPI and guarded RAM, not physical wire or firmware acceptance','output':r.stdout.strip(),'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(fixture.encode()).hexdigest()}
(ROOT/'reports/goodix-wire-stage63-tests.json').write_text(json.dumps(report,indent=2)+'\n');print(r.stdout,end='')
