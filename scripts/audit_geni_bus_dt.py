#!/usr/bin/env python3
"""Check selected active GENI resource references against saved AVR DT."""
from pathlib import Path
import hashlib,json,re,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
dma_source=(ROOT/'ports/geni-bus/msm_gpi.c').read_text()
DMA_ARGS=int(re.search(r'#define REQ_OF_DMA_ARGS \((\d+)\)',dma_source).group(1))
DMA_CHANNELS=int(re.search(r'#define MAX_CHANNELS_PER_GPII \((\d+)\)',dma_source).group(1))
WANTED={'qcom,gpi-dma','qcom,geni-se-qup','qcom,i2c-geni','qcom,spi-geni'}
def cells(b):
 if len(b)%4:raise ValueError('unaligned cell array')
 return list(struct.unpack('>'+str(len(b)//4)+'I',b))
def strings(b):return b.rstrip(b'\0').decode().split('\0')
inputs={};errors=[]
for tag,name in [('saved','device-baseline/running.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw);ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
  return True
 nodes={};counts={c:0 for c in WANTED};checks=[]
 def need(ok,description):
  checks.append({'check':description,'passed':bool(ok)})
  if not ok:errors.append(tag+': '+description)
 spec={'clocks':'#clock-cells','resets':'#reset-cells','dmas':'#dma-cells','iommus':'#iommu-cells','interconnects':'#interconnect-cells'}
 for n,p in tree.items():
  compat=set(strings(p.get('compatible',b'')))&WANTED
  if not compat or not active(n):continue
  for c in compat:counts[c]+=1
  normalized={}
  for prop,value in p.items():
   if prop in ('phandle','linux,phandle'):continue
   if prop in spec:
    values=cells(value);refs=[]
    while values:
     handle=values.pop(0);target=ph.get(handle);need(target is not None,n+': resolve '+prop)
     if target is None:break
     num=cells(tree[target].get(spec[prop],b''));need(len(num)==1 and num[0]<=len(values),n+': '+prop+' argument count')
     if len(num)!=1 or num[0]>len(values):break
     args=values[:num[0]];del values[:num[0]];need(active(target),n+': active '+prop+' provider '+target)
     if prop=='dmas':need(num[0]==DMA_ARGS and args[0]<DMA_CHANNELS,n+': GPI factory argument count and channel range')
     refs.append({'node':target,'args':args})
    normalized[prop]=refs
   elif prop.startswith('pinctrl-') and prop!='pinctrl-names' or prop in ('memory-region','interrupt-parent'):
    refs=[ph.get(i) for i in cells(value)];need(all(t is not None and active(t) for t in refs),n+': active reference '+prop);normalized[prop]=refs
   else:normalized[prop]=value.hex()
  if compat & {'qcom,i2c-geni','qcom,spi-geni'}:
   parent=n.rsplit('/',1)[0];need('qcom,geni-se-qup' in strings(tree[parent].get('compatible',b'')),n+': native GENI wrapper parent')
   need('se-clk' in strings(p.get('clock-names',b'')),n+': serial clock name')
   need('tx' in strings(p.get('dma-names',b'')) and 'rx' in strings(p.get('dma-names',b'')),n+': both DMA channels')
  nodes[n]=normalized
 inputs[tag]={'file':name,'sha256':hashlib.sha256(raw).hexdigest(),'active_compatible_counts':counts,'nodes':nodes,'checks':checks}
identical=inputs['saved']['nodes']==inputs['candidate']['nodes']
if not identical:errors.append('selected normalized resource properties differ from saved running DT')
r={'scope':'Active GENI resource shapes/targets and factory DMA spec checks only; no transfers, firmware, DMA mapping or interrupt proof','inputs':inputs,'saved_semantics_preserved':identical,'passed':not errors,'errors':errors,'whole_board_accepted':False,'physical_verified':False,'dt_modified':False}
(ROOT/'reports/geni-bus-stage68-dt.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({'passed':r['passed'],'counts':inputs['candidate']['active_compatible_counts'],'errors':errors},indent=2));raise SystemExit(bool(errors))
