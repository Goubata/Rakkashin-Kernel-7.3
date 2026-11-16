#!/usr/bin/env python3
"""Check saved DDR cooling DT semantics against the Stage97 private fixture; never write DT."""
from pathlib import Path
import argparse,hashlib,json,re,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from audit_boot_tree import fdt
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'reports/ddr-cooling-stage97-dt.json');a=p.parse_args()
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned DT cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
errors=[];checks=[]
def need(ok,msg):
 checks.append({'check':msg,'passed':bool(ok)})
 if not ok:errors.append(msg)
raw=(ROOT/'device-baseline/running.dtb').read_bytes();tree,_=fdt(raw)
ph={cells(v['phandle'])[0]:n for n,v in tree.items() if 'phandle' in v}
def active(n):
 while n:
  if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'ok',b'okay'):return False
  n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
 return True
nodes=[n for n,v in tree.items() if 'qcom,ddr-cooling-device' in strings(v.get('compatible',b'')) and active(n)]
need(len(nodes)==1,'one active qcom,ddr-cooling-device node')
node=nodes[0];v=tree[node]
need(cells(v.get('#cooling-cells',b''))==[2],'#cooling-cells is 2')
width=cells(v.get('qcom,bus-width',b''));table=cells(v.get('qcom,freq-table',b''))
need(width==[4],'bus width 4');need(table==[888],'factory frequency table <888>')
votes=[0]+[f*width[0] for f in table];need(votes==[0,3552],'driver vote table 0/3552 matches fixture expectation')
icc=cells(v.get('interconnects',b''));need(len(icc)==4 and icc[0]==icc[2],'one provider path with two endpoints')
provider=ph.get(icc[0]);pv=tree.get(provider,{})
need('qcom,alor-mc_virt' in strings(pv.get('compatible',b'')),'path provider is qcom,alor-mc_virt')
need(cells(pv.get('#interconnect-cells',b''))==[1],'provider uses one interconnect cell')
hdr=(ROOT/'ports/qcom-interconnect/include/dt-bindings/interconnect/qcom,alor.h').read_text()
ids={int(m.group(2)):m.group(1) for m in re.finditer(r'#define\s+(\w+)\s+(\d+)\b',hdr)}
endpoints=[ids.get(icc[1]),ids.get(icc[3])];need(endpoints==['MASTER_LLCC','SLAVE_EBI1'],'path is MASTER_LLCC -> SLAVE_EBI1')
me=cells(v['phandle'])[0];maps=[]
for n,m in tree.items():
 spec=cells(m.get('cooling-device',b'')) if 'cooling-device' in m else []
 if spec[:1]==[me]:
  trip=ph.get(cells(m['trip'])[0]);zone=n.split('/cooling-maps/')[0]
  maps.append({'map':n,'zone':zone,'min':spec[1],'max':spec[2],'trip':trip,'temperature':cells(tree[trip]['temperature'])[0],'hysteresis':cells(tree[trip]['hysteresis'])[0],'type':strings(tree[trip]['type'])[0],'zone_sensors':cells(tree[zone].get('thermal-sensors',b''))})
need(len(maps)==1,'one saved cooling-map references ddr_cdev')
need(all(m['min']==1 and m['max']==1 and m['min']<=len(table) for m in maps),'map selects state 1 within max_state')
need(all(m['type']=='passive' for m in maps),'map trip is passive')
fixture=(ROOT/'tests/ddr-cooling/qemu-ddr.dtso').read_text()
need('qcom,bus-width=<4>; qcom,freq-table=<888>; interconnects=<&avr_ddr_icc 3 &avr_ddr_icc 512>' in fixture,'private fixture reuses saved width/table/endpoint IDs')
report={'scope':'saved 6.12 DT read-only semantic audit; no phone DT edit','dtb_sha256':hashlib.sha256(raw).hexdigest(),'node':node,'bus_width':width,'freq_table':table,'driver_votes':votes,'provider':provider,'endpoints':endpoints,'maps':maps,'checks':checks,'passed':not errors,'errors':errors}
a.output.write_text(json.dumps(report,indent=1)+'\n');print(json.dumps({'passed':not errors,'errors':errors,'maps':maps},indent=1))
raise SystemExit(bool(errors))
