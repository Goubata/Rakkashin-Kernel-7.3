#!/usr/bin/env python3
"""Audit saved AVR QMI thermal contracts; no DT mutation or firmware assertions."""
from pathlib import Path
import hashlib,json,re,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'ports/qcom-qmi-cooling/qmi_cooling.c').read_text()
whitelist=set(re.findall(r'\{"([a-z0-9_]+)"\}',source))
paths=['device-baseline/running.dtb','artifacts/dt-bcl-name-stage91/avr-bcl-name-candidate.dtb']
root='/soc/qmi-tmd-devices';results=[];contracts=[];map_contracts=[]
def cell(b):return list(struct.unpack('>'+str(len(b)//4)+'I',b))
for path in paths:
 raw=(ROOT/path).read_bytes();tree=fdt(raw)[0];assert b'qcom,qmi-cooling-devices\0' in tree[root]['compatible']
 selected={n:p for n,p in tree.items() if n==root or n.startswith(root+'/')}
 rows=[];instances=set()
 for n,p in selected.items():
  if 'qcom,instance-id' not in p:continue
  inst=cell(p['qcom,instance-id']);assert len(inst)==1 and inst[0]<=0xffffff and inst[0] not in instances;instances.add(inst[0]);names=set()
  for child,props in selected.items():
   if child.rsplit('/',1)[0]!=n:continue
   wire=props['qcom,qmi-dev-name'].rstrip(b'\0').decode();assert wire in whitelist and wire not in names and len(wire)<=32;names.add(wire)
   assert cell(props['#cooling-cells'])==[2]
   rows.append({'node':child,'instance':inst[0],'wire_name':wire,'cooling_cells':2})
  assert names
 assert len(rows)==27 and instances=={0,0x43}
 ph={cell(props['phandle'])[0]:n for n,props in tree.items() if 'phandle' in props}
 maps=[]
 for n,props in tree.items():
  if 'cooling-device' not in props:continue
  data=cell(props['cooling-device']);idx=0
  while idx<len(data):
   target=ph[data[idx]];count=cell(tree[target]['#cooling-cells'])[0]
   assert idx+1+count<=len(data)
   if target.startswith(root+'/'):maps.append({'map':n,'target':target,'limits':data[idx+1:idx+1+count]})
   idx+=1+count
 map_contracts.append(sorted(maps,key=lambda m:(m['map'],m['target'])))
 # Compare every QMI property excluding renumbered numeric phandle identities; this port needs no rename.
 contracts.append({n:{k:v for k,v in props.items() if k not in ('phandle','linux,phandle')} for n,props in selected.items()})
 results.append({'file':path,'sha256':hashlib.sha256(raw).hexdigest(),'instances':sorted(instances),'cooling_devices':rows,'normalized_cooling_map_references':maps})
assert contracts[0]==contracts[1] and map_contracts[0]==map_contracts[1]
print(json.dumps({'passed':True,'scope':'27 saved DT cooling declarations match whitelist and parser shape; no firmware or physical behavior proof','qmi_subtree_properties_unchanged_except_phandle_ids':True,'inputs':results,'pending':['Native OF probe fixture and cooling-map runtime binding','Firmware advertisement/capabilities and physical modem/CDSP mitigation','Full-board schema and boot acceptance']},indent=2))
