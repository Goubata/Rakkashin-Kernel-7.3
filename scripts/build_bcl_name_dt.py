#!/usr/bin/env python3
"""Prepare a separate BCL-name DT candidate; preserve all physical properties."""
from pathlib import Path
import hashlib,json,re,struct
from audit_boot_tree import fdt
from build_dt_components import run,inside,DTC
ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb'
OUT=ROOT/'artifacts/dt-bcl-name-stage91';OUT.mkdir(exist_ok=True)
renames={'/soc/thermal-zones/pmih010x-2s-ibat-lvl'+str(i):'/soc/thermal-zones/pmih010x-2s-ibat'+str(i) for i in range(2)}
raw=BASE.read_bytes();before,_=fdt(raw)
assert set(renames)<=set(before)
assert not set(renames.values()) & set(before)
for n,p in before.items():
 if n.rsplit('/',1)[0]=='/soc/thermal-zones' and len(n.rsplit('/',1)[1])>=20:assert n in renames,n
def renamed(path):
 for old,new in renames.items():
  if path==old or path.startswith(old+'/'):return new+path[len(old):]
 return path
symbol_updates=set()
def prop_value(path,name,data):
 for old,new in renames.items():
  oldb=old.encode();newb=new.encode()
  if data.startswith(oldb) and data[len(oldb):len(oldb)+1] in (b'\0',b'/'):
   assert path in ('/__symbols__','/aliases'),(path,name)
   symbol_updates.add((path,name));return newb+data[len(oldb):]
 return data
expected={renamed(n):{k:prop_value(n,k,v) for k,v in p.items()} for n,p in before.items()}
# Native BCL maps voltage trip indices to hardware registers. Restore saved
# immediate trip-child order before renaming; sibling permutations retain all
# property bytes. Apply disjoint spans backwards so offsets remain valid.
original=raw
live,_=fdt((ROOT/'device-baseline/running.dtb').read_bytes())
h0=list(struct.unpack_from('>10I',raw));off,size=h0[2],h0[9];body0=raw[off:off+size]
stack=[];starts={};spans={};pos=0
while pos<len(body0):
 begin=pos;token=struct.unpack_from('>I',body0,pos)[0];pos+=4
 if token==1:
  end=body0.index(b'\0',pos);stack.append(body0[pos:end].decode());pos=(end+4)&~3;starts['/'+'/'.join(stack[1:])]=begin
 elif token==2:
  path='/'+'/'.join(stack[1:]);spans[path]=(starts[path],pos);stack.pop()
 elif token==3:
  length=struct.unpack_from('>I',body0,pos)[0];pos=(pos+8+length+3)&~3
 elif token==4:pass
 elif token==9:break
 else:raise ValueError('bad raw FDT token')
changes=[];edits=[];unmatched=[]
for parent in before:
 if not parent.startswith('/soc/thermal-zones/') or not parent.endswith('/trips'):continue
 current=[n for n in before if n.rsplit('/',1)[0]==parent]
 desired=[n for n in live if n.rsplit('/',1)[0]==parent]
 if set(current)!=set(desired):unmatched.append(parent);continue
 if current==desired or not current:continue
 for a,b in zip(current,current[1:]):assert spans[a][1]==spans[b][0],parent
 start,end=spans[current[0]][0],spans[current[-1]][1]
 replacement=b''.join(body0[spans[n][0]:spans[n][1]] for n in desired)
 assert len(replacement)==end-start
 edits.append((start,end,replacement));changes.append({'parent':parent,'before':current,'after':desired})
reordered=body0
for a,b,chunk in sorted(edits,reverse=True):reordered=reordered[:a]+chunk+reordered[b:]
raw=raw[:off]+reordered+raw[off+size:]
ordered,_=fdt(raw);assert ordered==before
expected={renamed(n):{k:prop_value(n,k,v) for k,v in props.items()} for n,props in ordered.items()}
header=list(struct.unpack_from('>10I',raw));so,stroff,rsvoff,ss=header[2],header[3],header[4],header[9]
assert len(raw)==header[1] and rsvoff<so and stroff>=so+ss
body=raw[so:so+ss];strings=raw[stroff:stroff+header[8]];pos=0;stack=[];parts=[]
while pos<len(body):
 start=pos;token=struct.unpack_from('>I',body,pos)[0];pos+=4
 if token==1:
  end=body.index(b'\0',pos);name=body[pos:end].decode();pos=(end+4)&~3;stack.append(name);path='/'+'/'.join(stack[1:])
  new=renamed(path).rsplit('/',1)[-1] if path in renames else name
  data=new.encode()+b'\0';parts.append(struct.pack('>I',token)+data+b'\0'*((-len(data))%4))
 elif token==2:stack.pop();parts.append(body[start:pos])
 elif token==3:
  size,off=struct.unpack_from('>II',body,pos);pos+=8;data=body[pos:pos+size];pos=(pos+size+3)&~3
  name=strings[off:strings.index(b'\0',off)].decode();path='/'+'/'.join(stack[1:]);data=prop_value(path,name,data)
  parts.append(struct.pack('>III',token,len(data),off)+data+b'\0'*((-len(data))%4))
 elif token==4:parts.append(body[start:pos])
 elif token==9:parts.append(body[start:]);break
 else:raise ValueError('invalid FDT token')
new_body=b''.join(parts);delta=len(new_body)-ss;result=bytearray(raw[:so]+new_body+raw[so+ss:])
header[1]+=delta;header[3]+=delta;header[9]+=delta;struct.pack_into('>10I',result,0,*header)
after,_=fdt(result);assert len(after)==len(before) and after==expected and list(after)==list(expected)
assert result[stroff+delta:]==raw[stroff:],'strings block changed'
assert result[rsvoff:so]==raw[rsvoff:so],'reservation/prefix changed'
GPU='/soc/qcom,kgsl-3d0@3d00000/qcom,gpu-pwrlevel-bins/qcom,gpu-pwrlevels-0'
assert len([n for n in after if n.rsplit('/',1)[0]==GPU])==21
assert all(after[n]==p for n,p in before.items() if renamed(n)==n and n not in ('/__symbols__','/aliases'))
candidate=OUT/'avr-bcl-name-candidate.dtb';candidate.write_bytes(result)
verified=OUT/'dtc-verified.dtb';run([DTC+'dtc','-I','dtb','-O','dtb','-o',inside(verified),inside(candidate)],OUT/'dtc.log')
check,_=fdt(verified.read_bytes());assert check==after
warnings=sorted(set(re.findall(r'Warning \(([^)]+)\)',(OUT/'dtc.log').read_text())))
sha=lambda b:hashlib.sha256(b).hexdigest()
r={'passed':True,'scope':'Separate native-name-length candidate, not whole-board DT or ROM acceptance','saved_order_file':'device-baseline/running.dtb','saved_order_sha256':sha((ROOT/'device-baseline/running.dtb').read_bytes()),'base_file':str(BASE.relative_to(ROOT)),'base_sha256':sha(original),'candidate_file':str(candidate.relative_to(ROOT)),'candidate_sha256':sha(result),'renames':renames,'symbol_path_properties_updated':sorted(symbol_updates),'all_hardware_property_values_preserved':True,'trip_subtrees_restored_to_saved_order':changes,'trip_groups_without_matching_saved_children':unmatched,'unrelated_node_order_preserved':True,'reservation_map_and_strings_preserved':True,'gpu_levels_unchanged':21,'native_dtc_passed':True,'dtc_warning_categories':warnings,'dt_schema_validated':False,'whole_board_dt_accepted':False,'thermal_abi_changed':False,'promoted':False,'physical_verified':False,'partition_written':False,'packaging_allowed':False,'required_pairing':'Vendor thermal HAL contains the old lvl0 name and an inlined C++ string length; DT must not be deployed alone','pending':['Paired HAL binary adjustment and ROM/HAL acceptance','SID4 conflict and all previous whole-board DT/boot gaps']}
(ROOT/'reports/bcl-names-stage91-dt.json').write_text(json.dumps(r,indent=2)+'\n');print('Prepared two-name candidate; restored',len(changes),'trip groups; all hardware property values and 21 GPU levels preserved')
