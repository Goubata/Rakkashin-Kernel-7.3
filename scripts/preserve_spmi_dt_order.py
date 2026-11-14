#!/usr/bin/env python3
"""Preserve saved running SPMI child order by permuting whole raw FDT subtrees."""
from pathlib import Path
import hashlib
import json
import struct
import subprocess
import sys
from audit_boot_tree import fdt
from build_dt_components import run, inside, DTC
ROOT=Path(__file__).resolve().parents[1]
TARGET='/soc/arbiter@c400000/spmi@c426000'
base=ROOT/'artifacts/dt-interconnect-stage18/avr-interconnect-candidate.dtb'
live=ROOT/'device-baseline/running.dtb'
raw=base.read_bytes();nodes,_=fdt(raw);running,_=fdt(live.read_bytes())
def children(tree):return [p for p in tree if p.rsplit('/',1)[0]==TARGET]
before=children(nodes);desired=[p for p in children(running) if p in before]
if set(desired)!=set(before):raise ValueError('Saved running tree cannot order all candidate SPMI children')
# Parse raw subtree spans. Header, reserve map, strings and every property byte
# stay intact; no bootloader-created node/property value is copied.
structoff=struct.unpack_from('>I',raw,8)[0];structsize=struct.unpack_from('>I',raw,36)[0]
body=raw[structoff:structoff+structsize];pos=0;stack=[];starts={};spans={}
while pos<len(body):
 start=pos;token=struct.unpack_from('>I',body,pos)[0];pos+=4
 if token==1:
  end=body.index(b'\0',pos);name=body[pos:end].decode();pos=(end+4)&~3
  stack.append(name);path='/'+'/'.join(stack[1:]);starts[path]=start
 elif token==2:
  path='/'+'/'.join(stack[1:]);spans[path]=(starts[path],pos);stack.pop()
 elif token==3:
  length=struct.unpack_from('>I',body,pos)[0];pos=(pos+8+length+3)&~3
 elif token==4:pass
 elif token==9:break
 else:raise ValueError('Unknown FDT token')
if not before:raise ValueError('No SPMI children')
for a,b in zip(before,before[1:]):
 if spans[a][1]!=spans[b][0]:raise ValueError('Unexpected inter-child data: refuse implicit editing')
start=spans[before[0]][0];end=spans[before[-1]][1]
replacement=b''.join(body[spans[p][0]:spans[p][1]] for p in desired)
if len(replacement)!=end-start:raise ValueError('Subtree permutation changed size')
result=raw[:structoff+start]+replacement+raw[structoff+end:]
changed,_=fdt(result)
if changed!=nodes:raise ValueError('Property/node content changed')
if children(changed)!=desired:raise ValueError('Child order does not match the saved running tree')
out=ROOT/'artifacts/dt-spmi-stage19';out.mkdir(parents=True,exist_ok=True)
output=out/'avr-spmi-candidate.dtb';output.write_bytes(result)
verified=out/'dtc-verified.dtb'
run([DTC+'dtc','-I','dtb','-O','dtb','-o',inside(verified),inside(output)],out/'dtc.log')
roundtrip,_=fdt(verified.read_bytes())
if roundtrip!=nodes or children(roundtrip)!=desired:raise ValueError('Native DTC changed semantics/order')
for script in ['reconstruct_gpu_dt.py','reconstruct_memory_dt.py']:
 subprocess.run([sys.executable,str(ROOT/'scripts'/script),'--verify-merged',str(output)],check=True)
r={'scope':'candidate child-order compatibility only; duplicate SID remains and physical PMIC probe is not verified','parent':TARGET,'before':before,'after':desired,'node_and_property_bytes_unchanged':True,'header_reserve_map_and_strings_preserved':True,'native_dtc_validation_passed':True,'saved_gpu_and_memory_preserved':True,'copied_running_values':False,'physical_verified':False,'packaging_allowed':False,'sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [base,live,output]}}
(ROOT/'reports/spmi-dt-order-repair.json').write_text(json.dumps(r,indent=2)+'\n')
print('Preserved',len(before),'SPMI child subtrees in saved running order; all node/property bytes unchanged')
