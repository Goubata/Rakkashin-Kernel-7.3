#!/usr/bin/env python3
"""Check saved/candidate AVR TRNG resource/optional clock shape; no MMIO."""
from pathlib import Path
import hashlib,json,re,struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
source=(ROOT/'ports/qcom-rng/qcom-rng.c').read_text()
assert re.search(r'\.compatible\s*=\s*"qcom,trng",\s*\.data\s*=\s*&qcom_trng_match_data',source)
assert re.search(r'qcom_trng_match_data\s*=\s*\{\s*\.hwrng_support\s*=\s*true',source)
assert 'devm_clk_get_optional(&pdev->dev, "core")' in source
max_reg=max(int(re.search(r'#define '+n+r'\s+(0x[\da-fA-F]+)',source).group(1),16) for n in ['PRNG_DATA_OUT','PRNG_STATUS'])
errors=[];inputs={}
for tag,name in [('saved','device-baseline/running.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw);ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
  return True
 nodes={};checks=[]
 def need(ok,msg):
  checks.append({'check':msg,'passed':bool(ok)})
  if not ok:errors.append(tag+': '+msg)
 for n,p in tree.items():
  if 'qcom,trng' not in strings(p.get('compatible',b'')) or not active(n):continue
  parent=n.rsplit('/',1)[0] or '/';reg=cells(p.get('reg',b''))
  need(cells(tree[parent].get('#address-cells',b''))==[2] and cells(tree[parent].get('#size-cells',b''))==[2],n+': two-cell address/size parent')
  need(len(reg)==4 and reg[2]==0 and reg[3]>=max_reg+4,n+': one resource covering DATA_OUT/STATUS')
  clocks=[];spec=cells(p.get('clocks',b''))
  while spec:
   target=ph.get(spec.pop(0));need(target is not None and active(target),n+': active clock provider')
   if target is None:break
   arity=cells(tree[target].get('#clock-cells',b''));need(len(arity)==1 and arity[0]<=len(spec),n+': clock arguments')
   if len(arity)!=1 or arity[0]>len(spec):break
   clocks.append([target]+spec[:arity[0]]);del spec[:arity[0]]
  need(not clocks or 'core' in strings(p.get('clock-names',b'')),n+': optional native core clock name')
  props={k:v.hex() for k,v in p.items() if k not in ('phandle','linux,phandle','clocks')};props['clock-providers']=clocks
  nodes[n]=props
 need(len(nodes)==1,'exactly one active AVR TRNG')
 inputs[tag]={'file':name,'sha256':hashlib.sha256(raw).hexdigest(),'nodes':nodes,'checks':checks}
identical=inputs['saved']['nodes']==inputs['candidate']['nodes']
if not identical:errors.append('Saved/candidate selected TRNG semantics differ')
r={'scope':'TRNG compatible/register/optional-clock shape and selected saved semantics only; not full schema, hardware availability or entropy-quality proof','passed':not errors,'inputs':inputs,'source_sha256':hashlib.sha256(source.encode()).hexdigest(),'saved_semantics_preserved':identical,'dt_modified':False,'physical_verified':False,'whole_board_accepted':False,'errors':errors}
(ROOT/'reports/rng-stage83-dt.json').write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({'passed':r['passed'],'nodes':list(inputs['candidate']['nodes']),'errors':errors},indent=2));raise SystemExit(bool(errors))
