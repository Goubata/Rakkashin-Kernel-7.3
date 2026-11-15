#!/usr/bin/env python3
"""Check saved watchdog/SDEI DT resources against the port; never read MMIO."""
from pathlib import Path
import hashlib,json,re,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned DT cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
source=(ROOT/'ports/qcom-watchdog/qcom_soc_wdt.c').read_text();core=(ROOT/'ports/qcom-watchdog/qcom_wdt_core.c').read_text()
body=re.search(r'wdt_default_map = \{(.*?)\n\};',source,re.S).group(1)
defaults={k:int(v,0) for k,v in re.findall(r'\.(\w+)\s*=\s*(0x[\da-fA-F]+|\d+)',body)}
assert 'irq_get_trigger_type(wdog_dd->bark_irq) | IRQF_NO_SUSPEND' in core
inputs={};errors=[]
for tag,name in [('saved','device-baseline/running.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw);ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
  return True
 checks=[];nodes={};sdei_nodes={}
 def need(ok,msg):
  checks.append({'check':msg,'passed':bool(ok)})
  if not ok:errors.append(tag+': '+msg)
 for n,p in tree.items():
  comp=strings(p.get('compatible',b''))
  if not active(n):continue
  if any(c in comp for c in ['qcom,sdei','arm,sdei-1.0']):
   sdei_nodes[n]={k:v.hex() for k,v in p.items() if k not in ('phandle','linux,phandle')}
  if 'qcom,msm-watchdog' not in comp:continue
  need(strings(p.get('reg-names',b''))==['wdt-base'],n+': named MMIO resource')
  reg=cells(p.get('reg',b''));need(len(reg)==4 and reg[2]==0,n+': one 64-bit resource')
  parent=n.rsplit('/',1)[0] or '/';need(cells(tree[parent].get('#address-cells',b''))==[2] and cells(tree[parent].get('#size-cells',b''))==[2],n+': two-cell address and size parent')
  hz=cells(p.get('qcom,wdt-hz',struct.pack('>I',defaults['wdt_hz'])));need(len(hz)==1 and hz[0]>0,n+': nonzero watchdog clock')
  for prop,key in [('qcom,wdt-en-offset','wdt0_en'),('qcom,wdt-rst-offset','wdt0_rst'),('qcom,wdt-sts-offset','wdt0_sts'),('qcom,wdt-bark-offset','wdt0_bark_time'),('qcom,wdt-bite-offset','wdt0_bite_time')]:
   offset=cells(p.get(prop,struct.pack('>I',defaults[key])))
   need(len(offset)==1 and offset[0]%4==0 and len(reg)==4 and reg[3]>=4 and offset[0]<=reg[3]-4,n+': bounded '+prop)
  bark=cells(p.get('qcom,bark-time',b''));pet=cells(p.get('qcom,pet-time',b''))
  need(len(bark)==len(pet)==1 and 0<pet[0]<bark[0]<=0xffffffff-3000,n+': explicit saved pet-before-bark interval')
  if len(bark)==len(hz)==1:
   need(0<bark[0]*hz[0]//1000<=0xffffffff and 0<(bark[0]+3000)*hz[0]//1000<=0xffffffff,n+': bark/bite fit hardware counters')
  irq=cells(p.get('interrupts',b''));need(len(irq)==6 and irq[0]==irq[3]==0 and irq[2]==irq[5]==4,n+': two level-high GIC SPIs')
  ancestor=n;target=None
  while ancestor:
   if 'interrupt-parent' in tree[ancestor]:
    value=cells(tree[ancestor]['interrupt-parent']);target=ph.get(value[0]) if len(value)==1 else None;break
   ancestor=ancestor.rsplit('/',1)[0] or ('/' if ancestor!='/' else '')
  need(target is not None and active(target) and cells(tree[target].get('#interrupt-cells',b''))==[3],n+': active GIC parent')
  props={k:v.hex() for k,v in p.items() if k not in ('phandle','linux,phandle','interrupt-parent')};props['irq-parent-node']=target;nodes[n]=props
 need(len(nodes)==1,'one active saved watchdog')
 inputs[tag]={'file':name,'sha256':hashlib.sha256(raw).hexdigest(),'watchdog_nodes':nodes,'sdei_nodes':sdei_nodes,'checks':checks}
identical=all(inputs['saved'][k]==inputs['candidate'][k] for k in ['watchdog_nodes','sdei_nodes'])
if not identical:errors.append('Selected saved/candidate watchdog/SDEI semantics differ')
r={'scope':'Saved watchdog register/clock/IRQ/timeout shape and SDEI presence only; no firmware reset, IRQ delivery or complete schema proof','passed':not errors,'saved_semantics_preserved':identical,'inputs':inputs,'defaults':defaults,'dt_modified':False,'physical_verified':False,'whole_board_accepted':False,'errors':errors}
(ROOT/'reports/watchdog-stage84-dt.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({'passed':r['passed'],'watchdog_nodes':list(inputs['candidate']['watchdog_nodes']),'sdei_nodes':list(inputs['candidate']['sdei_nodes']),'errors':errors},indent=2));raise SystemExit(bool(errors))
