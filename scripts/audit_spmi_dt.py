#!/usr/bin/env python3
"""Validate selected SPMI DT shapes/IDs only; never read PMIC registers."""
import json
from pathlib import Path
import struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
def cells(raw):
 if len(raw)%4:raise ValueError('Unaligned DT property')
 return struct.unpack('>'+str(len(raw)//4)+'I',raw)
def parent(path):return path.rsplit('/',1)[0] or '/' if path!='/' else None
report={'scope':'active compatible, bus/device ID and IRQ shape checks only; no PMIC probe, MMIO, transactions or interrupt delivery','physical_verified':False,'inputs':{}}
for key,name in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb'),('running','device-baseline/running.dtb')]:
 tree,_=fdt((ROOT/name).read_bytes());ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(path):
  while path is not None:
   if tree[path].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   path=parent(path)
  return True
 def compatible(path):return tree[path].get('compatible',b'').rstrip(b'\0').decode().split('\0')
 controllers={p:compatible(p) for p in tree if active(p) and any(c in ['qcom,canoe-spmi-pmic-arb','qcom,spmi-pmic-arb','qcom,x1e80100-spmi-pmic-arb','qcom,spmi-pmic-arb-debug','qcom,spmi-pmic-arb-debug-v7','qcom,spmi-pmic-arb-debug-v8'] for c in compatible(p))}
 buses={p for p in tree if active(p) and (p in controllers or parent(p) in controllers) and (p.rsplit('/',1)[-1].split('@')[0]=='spmi' or 'qcom,spmi-pmic-arb' in compatible(p))}
 pmics=[];errors=[];seen=set();first={};irqs=[]
 for p,props in tree.items():
  if not active(p):continue
  if parent(p) in buses and 'qcom,spmi-pmic' in compatible(p):
   reg=cells(props.get('reg',b''))
   if len(reg)!=2 or reg[0]>=16 or reg[1]!=0:errors.append({'path':p,'reason':'Native 7.3 child enumeration requires SID<16 and SPMI_USID=0','reg':reg});continue
   item=(parent(p),reg[0])
   if item in seen:errors.append({'path':p,'reason':'Duplicate active SID on one bus'})
   seen.add(item);first.setdefault(parent(p)+':'+str(reg[0]),p);pmics.append({'path':p,'sid':reg[0],'can_sleep':'qcom,can-sleep' in props})
  if 'interrupts' not in props:continue
  cur=p;domain=None
  while cur is not None:
   if 'interrupt-parent' in tree[cur]:
    ids=cells(tree[cur]['interrupt-parent'])
    if len(ids)==1:domain=ph.get(ids[0])
    break
   cur=parent(cur)
   if cur and '#interrupt-cells' in tree[cur]:domain=cur;break
  if domain not in buses:continue
  raw=cells(props['interrupts'])
  if cells(tree[domain].get('#interrupt-cells',b''))!=(4,) or len(raw)%4:
   errors.append({'path':p,'reason':'SPMI IRQ requires four-cell tuples'});continue
  for i in range(0,len(raw),4):
   sid,periph,index,trigger=raw[i:i+4]
   if sid>31 or periph>255 or index>7:errors.append({'path':p,'reason':'SPMI IRQ field outside driver bit width','args':raw[i:i+4]})
   irqs.append({'path':p,'domain':domain,'args':raw[i:i+4]})
 if not controllers or not pmics:errors.append({'reason':'No active target SPMI controllers/PMICs selected'})
 report['inputs'][key]={'controllers':controllers,'bus_count':len(buses),'active_pmic_count':len(pmics),'max_selected_sid':max((x['sid'] for x in pmics),default=None),'native_16_sid_enumeration_sufficient':all(x['sid']<16 for x in pmics),'pmics':pmics,'first_selected_by_bus_sid':first,'irq_reference_count':len(irqs),'irq_references':irqs,'errors':errors}
report['first_selection_matches_running']={n:report['inputs'][n]['first_selected_by_bus_sid']==report['inputs']['running']['first_selected_by_bus_sid'] for n in ['saved','candidate']}
report['passed']=not any(r['errors'] for r in report['inputs'].values())
(ROOT/'reports/spmi-dt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({key:{k:v for k,v in r.items() if k not in ('pmics','irq_references','first_selected_by_bus_sid')} for key,r in report['inputs'].items()},indent=2))
raise SystemExit(not report['passed'])
