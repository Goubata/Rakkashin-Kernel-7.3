#!/usr/bin/env python3
"""Compare factory Goodix UAPI declarations/build features and selected stock code."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
PIN='a24357771fe451d829a2f78f8986ecd53522085e'
REPO=ROOT.parent/'avr-upstream-audit/vendor-repos/motorola-kernel-modules'
def original(path):return subprocess.check_output(['git','-C',str(REPO),'show',PIN+':'+path])
def normalize(raw):return ' '.join(re.sub(r'/\*.*?\*/','',raw.decode(),flags=re.S).split())
source='drivers/input/touchscreen/goodix_thp/goodix_thp.h'
a=normalize(original(source));b=normalize((ROOT/'ports/goodix-touch/goodix_thp/goodix_thp.h').read_bytes())
for declaration in ['int goodix_thp_enter_tui(void);','int goodix_thp_exit_tui(void);']:b=b.replace(declaration,'')
assert a==' '.join(b.split())
source='include/linux/touchscreen_mmi.h';header=original(source)
assert header==(ROOT/'ports/goodix-touch'/source).read_bytes()
features={}
makefile=(ROOT/'ports/goodix-touch/Makefile').read_text()
for folder in ['goodix_thp','touchscreen_mmi']:
    raw=original('drivers/input/touchscreen/'+folder+'/BUILD.bazel').decode()
    block=re.search(r'local_defines\s*=\s*\[(.*?)\]',raw,re.S).group(1)
    flags=re.findall(r'"(CONFIG_\w+)"',block)
    assert all('-D'+flag+'=1' in makefile for flag in flags)
    features[folder]=flags
stock=ROOT/'artifacts/goodix-stage54-stock/goodix_brl_thp.ko';raw=stock.read_bytes()
off=struct.unpack_from('<Q',raw,40)[0];ents,count=struct.unpack_from('<HH',raw,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',raw,off+i*ents) for i in range(count)]
code=None
for section in sections:
    if section[1]!=2:continue
    strings=sections[section[6]];names=raw[strings[4]:strings[4]+strings[5]]
    for pos in range(section[4],section[4]+section[5],section[9]):
        name,_,_,index,value,size=struct.unpack_from('<IBBHQQ',raw,pos)
        if names[name:].split(b'\0',1)[0]==b'goodix_thp_input_agent_ioctl_get_events':
            text=sections[index];code=raw[text[4]+value:text[4]+value+size]
# Actual archived ARM64 implementation: mov w0,wzr; ret. No payload is copied.
assert code==bytes.fromhex('e0031f2ac0035fd6')
r={'scope':'header-level UAPI declarations and selected stock function bytes/build flags only; not complete ioctl/userspace/hardware ABI proof','passed':True,'source_commit':PIN,'touchscreen_mmi_header_sha256':hashlib.sha256(header).hexdigest(),'goodix_header_only_adds_tui_prototypes':True,'factory_feature_defines':features,'stock_module_sha256':hashlib.sha256(raw).hexdigest(),'stock_get_events_bytes':code.hex(),'stock_get_events_semantics':'returns zero without event payload; preserve legacy no-op rather than claim event acquisition implemented','physical_verified':False,'pending':['Actual ioctl/compat_ioctl payloads, open/remove lifetime, frame bounds and concurrency','SPI/IRQ/display/power/gesture/stylus firmware and daemon integration','All remaining board/ROM/boot acceptance gates']}
(ROOT/'reports/goodix-touch-stage54-abi.json').write_text(json.dumps(r,indent=2)+'\n')
print('Verified Goodix header UAPI, all nine factory build flags and archived legacy GET_EVENTS no-op')
