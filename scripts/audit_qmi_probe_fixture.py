#!/usr/bin/env python3
"""Compare private OF-probe fixture to saved AVR QMI declarations, never hardware."""
from pathlib import Path
import hashlib,json,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
files={'saved':'device-baseline/running.dtb','guest':'artifacts/qemu-qmi-cooling/virt-rail.dtb'}
roots={'saved':'/soc/qmi-tmd-devices','guest':'/avr-qmi-fixtures/good'}
contracts={};results={}
for tag,path in files.items():
 raw=(ROOT/path).read_bytes();tree=fdt(raw)[0];root=roots[tag];rows=[]
 for n,props in tree.items():
  if n.rsplit('/',1)[0]!=root:continue
  instance=struct.unpack('>I',props['qcom,instance-id'])[0]
  for child,p in tree.items():
   if child.rsplit('/',1)[0]!=n:continue
   cells=struct.unpack('>I',p['#cooling-cells'])[0];assert cells==2
   rows.append({'instance':instance,'node_name':child.rsplit('/',1)[1],'wire_name':p['qcom,qmi-dev-name'].rstrip(b'\0').decode(),'cooling_cells':cells})
 contracts[tag]=sorted(rows,key=lambda r:(r['instance'],r['node_name']))
 assert len(rows)==27
 results[tag]={'file':path,'sha256':hashlib.sha256(raw).hexdigest(),'cooling_declarations':contracts[tag]}
assert contracts['saved']==contracts['guest']
print(json.dumps({'passed':True,'scope':'All 27 saved instance IDs, node/wire names and cooling arity reproduced in private OF probe fixture; physical firmware, cooling maps, IRQs and board not modeled','inputs':results},indent=2))
