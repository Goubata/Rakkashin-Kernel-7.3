#!/usr/bin/env python3
"""Audit selected saved thermal trips and QMI map projection in private guest DT."""
from pathlib import Path
import hashlib,json,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
files={'saved':'device-baseline/running.dtb','guest':'artifacts/qemu-qmi-cooling/virt-rail.dtb'}
names=['pmih010x-bcl-lvl0','pmih010x-bcl-lvl1','socd','mdmss-0'];results={};contracts={}
def cells(b):return list(struct.unpack('>'+str(len(b)//4)+'I',b))
for tag,path in files.items():
 raw=(ROOT/path).read_bytes();tree=fdt(raw)[0];ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p};rows=[]
 base='/soc/thermal-zones/' if tag=='saved' else '/thermal-zones/'
 qmi='/soc/qmi-tmd-devices/' if tag=='saved' else '/avr-qmi-fixtures/good/'
 for name in names:
  root=base+name;trips=[];maps=[]
  for n,p in tree.items():
   if n.rsplit('/',1)[0]==root+'/trips':trips.append({'name':n.rsplit('/',1)[1],'temperature':cells(p['temperature'])[0],'hysteresis':cells(p['hysteresis'])[0],'type':p['type'].rstrip(b'\0').decode()})
   if n.rsplit('/',1)[0]!=root+'/cooling-maps':continue
   values=cells(p['cooling-device']);refs=[];i=0
   while i<len(values):
    target=ph[values[i]];count=cells(tree[target]['#cooling-cells'])[0]
    assert i+1+count<=len(values)
    if target.startswith(qmi):refs.append({'target':target.removeprefix(qmi),'limits':values[i+1:i+1+count]})
    i+=1+count
   if refs:maps.append({'name':n.rsplit('/',1)[1],'trip':ph[cells(p['trip'])[0]].rsplit('/',1)[1],'qmi_targets':refs})
  assert trips and maps
  rows.append({'zone':name,'polling_delay_passive':cells(tree[root]['polling-delay-passive'])[0],'ordered_trips':trips,'maps':sorted(maps,key=lambda r:r['name'])})
 contracts[tag]=rows;results[tag]={'file':path,'sha256':hashlib.sha256(raw).hexdigest(),'zones':rows}
assert contracts['saved']==contracts['guest']
assert sum(len(m['qmi_targets']) for z in contracts['guest'] for m in z['maps'])==8
print(json.dumps({'passed':True,'scope':'Four selected zones retain saved trip order/scalars/types/passive delay and eight QMI cooling-map references with exact limits; other providers projected out and physical sensors replaced by private recorded values','inputs':results,'recorded_firmware_caps':{'modem_lte_dsc':255,'modem_nr_dsc':255,'modem_nr_scg_dsc':100,'cdsp_sw':4,'other_clients':3},'cap_scope':'Local test capability values chosen to represent saved DT limits; not physical firmware advertisement proof'},indent=2))
