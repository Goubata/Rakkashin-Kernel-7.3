#!/usr/bin/env python3
"""Inspect exact saved Alor ELF table relocations; never execute the 6.12 module."""
from pathlib import Path
import hashlib
import json
import struct
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'device-baseline/early-modules/0/lib/modules/qnoc-alor.ko'
d=p.read_bytes()
if d[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<H',d,18)[0]!=183:raise ValueError('Not ARM64 ELF64 LE')
off=struct.unpack_from('<Q',d,40)[0];size,count,nidx=struct.unpack_from('<HHH',d,58)
h=[struct.unpack_from('<IIQQQQIIQQ',d,off+i*size) for i in range(count)]
ns=d[h[nidx][4]:h[nidx][4]+h[nidx][5]]
names=[ns[x[0]:].split(b'\0',1)[0].decode() for x in h]
si=names.index('.symtab');st=h[h[si][6]];strings=d[st[4]:st[4]+st[5]];syms=[]
for o in range(h[si][4],h[si][4]+h[si][5],24):
 n,info,other,section,value,length=struct.unpack_from('<IBBHQQ',d,o)
 syms.append({'name':strings[n:].split(b'\0',1)[0].decode(),'section':section,'value':value,'size':length})
lookup={s['name']:s for s in syms if s['name']}
def entry(array,index):
 a=lookup[array]
 if index*8+8>a['size']:raise ValueError('Array slot outside symbol')
 target=a['value']+index*8;result=[]
 for sh in h:
  if sh[1]!=4 or sh[7]!=a['section']:continue
  for o in range(sh[4],sh[4]+sh[5],24):
   loc,info,addend=struct.unpack_from('<QQq',d,o)
   if loc==target:
    if info & 0xffffffff !=257:raise ValueError('Expected ABS64 pointer relocation')
    s=syms[info>>32]
    result += [n['name'] for n in syms if n['section']==s['section'] and n['value']==s['value']+addend and n['name']]
 raw=struct.unpack_from('<Q',d,h[a['section']][4]+target)[0]
 return {'array':array,'index':index,'raw_pointer_slot_zero':raw==0,'relocation_target_names':result}
rows=[entry('cnoc_cfg_nodes',523),entry('cnoc_main_nodes',523),entry('cnoc_cfg_nodes',540)]
assert rows[0]['raw_pointer_slot_zero'] and not rows[0]['relocation_target_names']
assert 'qhs_ipa' in rows[1]['relocation_target_names']
assert 'qhs_usb3' in rows[2]['relocation_target_names']
r={'scope':'saved ELF slot/relocation evidence only; not USB/IPA hardware execution','module':str(p.relative_to(ROOT)),'sha256':hashlib.sha256(d).hexdigest(),'entries':rows,'finding':'Saved USB usb-ipa endpoint points at an empty cnoc_cfg slot; the saved cnoc_main slot with the same ID resolves to qhs_ipa','physical_route_verified':False}
(ROOT/'reports/interconnect-saved-module-evidence.json').write_text(json.dumps(r,indent=2)+'\n')
print(r['finding'])
