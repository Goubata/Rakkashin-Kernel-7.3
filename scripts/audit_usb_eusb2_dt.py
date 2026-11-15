#!/usr/bin/env python3
"""Validate selected AVR M31/PMIC USB DT dependencies without touching hardware."""
from pathlib import Path
import hashlib,json,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
PHY='/soc/hsphy@88e3000';REP='/soc/arbiter@c400000/spmi@c426000/qcom,pmih010x@7/eusb2-repeater@fd00'
def cells(b):
 if len(b)%4:raise ValueError('unaligned cells')
 return list(struct.unpack('>'+str(len(b)//4)+'I',b))
def strings(b):
 if b and not b.endswith(b'\0'):raise ValueError('unterminated string list')
 return b.rstrip(b'\0').decode().split('\0') if b else []
inputs={};errors=[]
for tag,name in [('saved','device-baseline/running.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw);handles={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
  return True
 checks=[];providers={}
 def need(ok,reason):
  checks.append({'check':reason,'passed':bool(ok)})
  if not ok:errors.append(tag+': '+reason)
 for path in [PHY,REP]:need(path in tree and active(path),'active selected node '+path)
 phy=tree[PHY];rep=tree[REP]
 need('qcom,usb-m31-eusb2-phy' in strings(phy['compatible']),'M31 driver compatible')
 need('qcom,pmic-eusb2-repeater' in strings(rep['compatible']),'PMIC repeater driver compatible')
 need(cells(phy['usb-repeater'])==cells(rep['phandle']),'PHY phandle selects actual PMIC repeater')
 need(cells(tree[REP.rsplit('/',1)[0]]['reg'])==[7,0],'repeater belongs to SID7 USID provider')
 need(cells(rep['reg'])==[0xfd00],'PMIC register base matches saved device')
 names=strings(phy['reg-names']);need('eusb2_phy_base' in names and 'eud_detect_reg' in names,'required memory resource names')
 need(len(cells(phy['reg']))==4*len(names),'64-bit address/size resource tuple count')
 need('ref_clk_src' in strings(phy['clock-names']),'required reference clock name')
 need('phy_reset' in strings(phy['reset-names']),'required reset name')
 need(len(cells(phy['qcom,vdd-voltage-level']))==3,'complete three-state vdd voltage table')
 for prop,argcount in [('clocks','#clock-cells'),('resets','#reset-cells')]:
  values=cells(phy[prop]);refs=[]
  while values:
   handle=values.pop(0);node=handles.get(handle);need(node is not None,'resolve '+prop+' provider')
   if node is None:break
   counts=cells(tree[node].get(argcount,b''));need(len(counts)==1 and len(values)>=counts[0],'provider argument count '+prop)
   if len(counts)!=1 or len(values)<counts[0]:break
   args=values[:counts[0]];del values[:counts[0]];refs.append({'node':node,'args':args,'active':active(node)});need(active(node),'active '+prop+' provider '+node)
  providers[prop]=refs
 for path,props in [(PHY,['vdd-supply','vdda12-supply','vdd_refgen-supply']),(REP,['vdd3-supply','vdd18-supply'])]:
  for prop in props:
   ids=cells(tree[path].get(prop,b''));node=handles.get(ids[0]) if len(ids)==1 else None
   need(node is not None and active(node),'active supply '+prop+' for '+path)
   providers[path+':'+prop]=node
 for path in [PHY,REP]:
  for prop in ['qcom,param-override-seq','qcom,host-param-override-seq']:
   if prop in tree[path]:need(len(cells(tree[path][prop]))%2==0,'override pair count '+path+':'+prop)
 selected={}
 for n in [PHY,REP]:
  selected[n]={}
  for prop,value in tree[n].items():
   if prop in ('phandle','linux,phandle'):continue
   if prop.endswith('-supply') or prop=='usb-repeater':
    ids=cells(value);selected[n][prop]=[handles.get(i) for i in ids]
   elif prop in ('clocks','resets'):
    selected[n][prop]=providers[prop]
   else:selected[n][prop]=value.hex()
 inputs[tag]={'file':name,'sha256':hashlib.sha256(raw).hexdigest(),'checks':checks,'providers':providers,'selected_properties':selected}
unchanged=inputs['saved']['selected_properties']==inputs['candidate']['selected_properties']
if not unchanged:errors.append('selected USB node semantics differ from saved running tree')
r={'scope':'Selected DT compatibility, active parent/provider and tuple checks only; no clock/reset/regulator/PMIC/MMIO/USB probe proof','inputs':inputs,'selected_properties_unchanged':unchanged,'comparison_scope':'phandles normalized to target paths; clock/reset arguments and all non-phandle property bytes exact','passed':not errors,'errors':errors,'whole_board_accepted':False,'physical_verified':False,'dt_modified':False}
(ROOT/'reports/usb-eusb2-stage65-dt.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({'passed':r['passed'],'selected_properties_unchanged':unchanged,'comparison_scope':'phandles normalized to target paths; clock/reset arguments and all non-phandle property bytes exact','errors':errors},indent=2));raise SystemExit(bool(errors))
