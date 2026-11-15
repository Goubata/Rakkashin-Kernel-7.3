#!/usr/bin/env python3
"""Compare selected TSENS v2 controllers/zones against saved AVR DT; no MMIO."""
from pathlib import Path
import hashlib,json,re,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned DT cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
source=(ROOT/'ports/qcom-tsens/tsens-v2.c').read_text()
body=re.search(r'static struct tsens_features tsens_v2_feat = \{(.*?)\n\};',source,re.S).group(1)
limits={k:int(re.search(r'\.'+k+r'\s*=\s*(-?\d+)',body).group(1)) for k in ['max_sensors','trip_min_temp','trip_max_temp']}
inputs={};errors=[]
for tag,name in [('saved','device-baseline/running.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw)
 ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'ok',b'okay'):return False
   n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
  return True
 checks=[]
 def need(ok,msg):
  checks.append({'check':msg,'passed':bool(ok)})
  if not ok:errors.append(tag+': '+msg)
 controllers={};zones={}
 for n,p in tree.items():
  if 'qcom,tsens-v2' not in strings(p.get('compatible',b'')) or not active(n):continue
  count=cells(p.get('#qcom,sensors',b''));need(len(count)==1 and 1<=count[0]<=limits['max_sensors'],n+': supported sensor count')
  parent=n.rsplit('/',1)[0] or '/';need(cells(tree[parent].get('#address-cells',b''))==[2] and cells(tree[parent].get('#size-cells',b''))==[2],n+': two-cell address/size parent')
  reg=cells(p.get('reg',b''));need(len(reg)==8,n+': exactly indexed TM/SROT resources')
  if len(reg)==8:
   need(reg[0]==reg[2]==reg[4]==reg[6]==0 and reg[3]>=0x140 and reg[7]>=8,n+': resource size covers native v2 register fields')
  need(cells(p.get('#thermal-sensor-cells',b''))==[1],n+': single thermal sensor argument')
  need(strings(p.get('interrupt-names',b''))==['uplow','critical'],n+': native separate IRQ names')
  irq=cells(p.get('interrupts',b''));need(len(irq)==6 and irq[0]==irq[3]==0 and irq[2]==irq[5]==4,n+': two level-high GIC SPI specifiers')
  ancestor=n;handle=None
  while ancestor:
   if 'interrupt-parent' in tree[ancestor]:
    handles=cells(tree[ancestor]['interrupt-parent']);handle=handles[0] if len(handles)==1 else None;break
   ancestor=ancestor.rsplit('/',1)[0] or ('/' if ancestor!='/' else '')
  irqnode=ph.get(handle);need(irqnode is not None and active(irqnode) and cells(tree[irqnode].get('#interrupt-cells',b''))==[3],n+': active three-cell IRQ parent')
  normalized={k:v.hex() for k,v in p.items() if k not in ('phandle','linux,phandle','interrupt-parent')};normalized['interrupt-parent-node']=irqnode
  controllers[n]={'sensor_count':count[0] if len(count)==1 else None,'properties':normalized}
 for n,p in tree.items():
  if 'thermal-sensors' not in p or not active(n):continue
  spec=cells(p['thermal-sensors']);provider=ph.get(spec[0]) if spec else None
  if provider not in controllers:continue
  need(len(spec)==2 and 0<=spec[1]<controllers[provider]['sensor_count'],n+': sensor ID within controller count')
  props={k:v.hex() for k,v in p.items() if k not in ('phandle','linux,phandle','thermal-sensors')}
  trips={}
  for child,q in tree.items():
   if not child.startswith(n+'/trips/') or not active(child):continue
   normalized={k:v.hex() for k,v in q.items() if k not in ('phandle','linux,phandle')};trips[child]=normalized
   if 'temperature' in q:
    temp=struct.unpack('>i',q['temperature'])[0]
    need(limits['trip_min_temp']<=temp<=limits['trip_max_temp'],child+': trip lies within factory-preserved range')
  zones[n]={'sensor':[provider]+spec[1:],'properties':props,'trips':trips}
 for n,c in controllers.items():
  ids={z['sensor'][1] for z in zones.values() if z['sensor'][0]==n and len(z['sensor'])==2}
  need(ids==set(range(c['sensor_count'])),n+': all declared sensors have active thermal zones')
 inputs[tag]={'file':name,'sha256':hashlib.sha256(raw).hexdigest(),'controllers':controllers,'thermal_zones':zones,'controller_count':len(controllers),'declared_sensor_count':sum(c['sensor_count'] for c in controllers.values()),'zone_count':len(zones),'checks':checks}
identical=all(inputs['saved'][key]==inputs['candidate'][key] for key in ['controllers','thermal_zones'])
if not identical:errors.append('Selected saved and candidate TSENS semantics differ')
r={'scope':'Selected TSENS v2 node/register/IRQ/zone shapes and saved semantic preservation only; not full schema validation or physical thermal acceptance','passed':not errors,'limits':limits,'saved_semantics_preserved':identical,'inputs':inputs,'errors':errors,'dt_modified':False,'whole_board_accepted':False,'physical_verified':False}
(ROOT/'reports/tsens-stage78-dt.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:r[k] for k in ['passed','saved_semantics_preserved','errors']}|{k:inputs['candidate'][k] for k in ['controller_count','declared_sensor_count','zone_count']},indent=2));raise SystemExit(bool(errors))
