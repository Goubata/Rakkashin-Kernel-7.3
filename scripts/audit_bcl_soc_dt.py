#!/usr/bin/env python3
"""Check selected saved BCL depletion/cooling DT semantics; never write DT."""
from pathlib import Path
import argparse,hashlib,json,struct
p=argparse.ArgumentParser(description=__doc__);p.add_argument("--guest",action="store_true");args=p.parse_args()
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned DT cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
errors=[];inputs={}
for tag,name in [('saved','device-baseline/running.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw);ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p};checks=[];selected={}
 def need(ok,msg):
  checks.append({'check':msg,'passed':bool(ok)})
  if not ok:errors.append(tag+': '+msg)
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'ok',b'okay'):return False
   n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
  return True
 providers=[n for n,p in tree.items() if 'qcom,msm-bcl-soc' in strings(p.get('compatible',b'')) and active(n)]
 need(len(providers)==1,'one active BCL SoC provider')
 consumers=[]
 for provider in providers:
  pp=tree[provider];need(cells(pp.get('#thermal-sensor-cells',b''))==[0],provider+': zero sensor arguments')
  selected[provider]={k:v.hex() for k,v in pp.items() if k not in ('phandle','linux,phandle')}
  for n,p in tree.items():
   spec=cells(p.get('thermal-sensors',b''))
   if not spec or ph.get(spec[0])!=provider:continue
   need(active(n) and len(spec)==1,n+': active zero-argument consumer');consumers.append(n)
   selected[n]={k:v.hex() for k,v in p.items() if k not in ('thermal-sensors','phandle','linux,phandle')};selected[n]['sensor']=provider
   for delay in ('polling-delay','polling-delay-passive'):
    need(delay not in p or len(cells(p[delay]))==1,n+': optional single '+delay+' (native absence means zero)')
   trips=[];maps=[]
   for child,cp in tree.items():
    if child.startswith(n+'/trips/'):
     temp=cells(cp.get('temperature',b''));hyst=cells(cp.get('hysteresis',b''))
     need(len(temp)==len(hyst)==1 and 0<=temp[0]<=100 and 0<=hyst[0]<=temp[0],child+': depletion/hysteresis in percent units')
     need(strings(cp.get('type',b''))==['passive'],child+': passive depletion trip')
     selected[child]={k:v.hex() for k,v in cp.items() if k not in ('phandle','linux,phandle')};trips.append(child)
    if child.startswith(n+'/cooling-maps/'):
     trip=cells(cp.get('trip',b''));target=ph.get(trip[0]) if len(trip)==1 else None
     need(target is not None and target.startswith(n+'/trips/'),child+': local trip reference')
     cooling=cells(cp.get('cooling-device',b''));links=[]
     while cooling:
      node=ph.get(cooling.pop(0));need(node is not None and active(node),child+': active cooling provider')
      if node is None:break
      arity=cells(tree[node].get('#cooling-cells',b''));need(arity==[2] and len(cooling)>=2,child+': two cooling bounds')
      if arity!=[2] or len(cooling)<2:break
      low,high=cooling[:2];del cooling[:2];need(low<=high,child+': ordered cooling bounds');links.append([node,low,high])
     need(len(links)==3,child+': three saved cooling providers');selected[child]={'trip':target,'cooling':links,**{k:v.hex() for k,v in cp.items() if k not in ('trip','cooling-device','phandle','linux,phandle')}};maps.append(child)
   need(len(trips)==2 and len(maps)==1,n+': two trips and one cooling map')
 need(len(consumers)==1,'one selected depletion zone')
 inputs[tag]={'file':name,'sha256':hashlib.sha256(raw).hexdigest(),'selected':selected,'checks':checks}
preserved=inputs['saved']['selected']==inputs['candidate']['selected']
if not preserved:errors.append('selected saved/candidate semantics differ')
guest=None
if args.guest:
 name='artifacts/qemu-bcl-soc/virt-rail.dtb';raw=(ROOT/name).read_bytes();tree,_=fdt(raw)
 ph={cells(props['phandle'])[0]:n for n,props in tree.items() if 'phandle' in props}
 zone='/thermal-zones/avr-socd';sensor='/avr-bcl-private';props=tree[zone];checks=[]
 def check(ok,msg):
  checks.append({'check':msg,'passed':bool(ok)})
  if not ok:errors.append('guest: '+msg)
 saved=inputs['saved']['selected'];z='/soc/thermal-zones/socd'
 check(cells(tree[sensor]['#thermal-sensor-cells'])==[0] and len(cells(props['thermal-sensors']))==1 and ph[cells(props['thermal-sensors'])[0]]==sensor,'zero-argument native OF sensor shape')
 for delay in ('polling-delay-passive','polling-delay'):
  check(cells(props.get(delay,b'\0'*4))==cells(bytes.fromhex(saved[z].get(delay,'00000000'))),delay+' matches saved AVR semantics')
 def metric(p):return (tuple(cells(p['temperature'])),tuple(cells(p['hysteresis'])),strings(p['type']))
 guest_trips=sorted(metric(v) for n,v in tree.items() if n.startswith(zone+'/trips/'))
 saved_trips=sorted(metric({k:bytes.fromhex(v) for k,v in props.items()}) for n,props in saved.items() if n.startswith(z+'/trips/'))
 check(guest_trips==saved_trips,'trip metrics, hysteresis and types match saved AVR')
 maps=[v for n,v in tree.items() if n.startswith(zone+'/cooling-maps/')];check(len(maps)==1,'one cooling map')
 bounds=[]
 for m in maps:
  check(cells(tree[ph[cells(m['trip'])[0]]]['temperature'])==[95],'cooling map targets depletion 95 trip')
  spec=cells(m['cooling-device'])
  while spec:
   target=ph[spec.pop(0)];check(cells(tree[target]['#cooling-cells'])==[2],'two cooling arguments')
   bounds.append(spec[:2]);del spec[:2]
 saved_bounds=[link[1:] for n,props in saved.items() if n.startswith(z+'/cooling-maps/') for link in props['cooling']]
 check(sorted(bounds)==sorted(saved_bounds),'three cooling bounds match saved AVR; provider hardware deliberately private')
 guest={'file':name,'sha256':hashlib.sha256(raw).hexdigest(),'checks':checks,'scope':'Private guest fixture matches selected scalar binding shape only; substitute cooling providers do not emulate physical cooling'}
r={'passed':not errors,'scope':'Selected BCL SoC provider, zero-cell consumer, depletion trips and normalized cooling links only; not full DT schema or driver availability','inputs':inputs,'guest':guest,'saved_semantics_preserved':preserved,'dt_modified':False,'physical_verified':False,'whole_board_accepted':False,'errors':errors}
(ROOT/'reports/bcl-soc-stage87-dt.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({'passed':r['passed'],'errors':errors},indent=2));raise SystemExit(bool(errors))
