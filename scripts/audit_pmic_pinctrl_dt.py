#!/usr/bin/env python3
"""Check PMIC GPIO binding pin numbers only, without probing or changing pins."""
import json
from pathlib import Path
import re
import struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
providers={}
for kind in ['gpio','mpp']:
 s=(ROOT/('ports/pmic-pinctrl/pinctrl-spmi-'+kind+'.c')).read_text()
 for compat,count in re.findall(r'\.compatible\s*=\s*"([^"]+)"\s*,\s*\.data\s*=\s*\(void \*\)\s*(\d+)',s):providers[compat]={'kind':kind,'pin_count':int(count)}
def cells(raw):
 if len(raw)%4:raise ValueError('Unaligned DT cell array')
 return struct.unpack('>'+str(len(raw)//4)+'I',raw)
def parent(path):return path.rsplit('/',1)[0] or '/' if path!='/' else None
report={'scope':'selected compatible and qualified GPIO-phandle pin-range checks only; no valid-hole, routing, PMIC probe, electrical or IRQ proof','inputs':{},'physical_verified':False}
for key,path in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]:
 tree,_=fdt((ROOT/path).read_bytes());ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(p):
  while p is not None:
   if tree[p].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   p=parent(p)
  return True
 selected={}
 for n,p in tree.items():
  if not active(n):continue
  for c in p.get('compatible',b'').rstrip(b'\0').decode().split('\0'):
   if c in providers:selected[n]={'compatible':c,**providers[c]};break
 refs=[];errors=[];unparsed=[];pin_configs=[]
 for n,p in tree.items():
  if not active(n):continue
  for name,raw in p.items():
   if name!='gpios' and not name.endswith(('-gpio','-gpios')):continue
   if 'gpio-hog' in p:unparsed.append(n+':'+name);continue
   values=cells(raw);pos=0
   while pos<len(values):
    ident=values[pos];pos+=1
    if ident==0:continue
    target=ph.get(ident)
    if target is None or '#gpio-cells' not in tree[target]:unparsed.append(n+':'+name);break
    arity=cells(tree[target]['#gpio-cells'])
    if len(arity)!=1 or pos+arity[0]>len(values):raise ValueError('Malformed GPIO phandle tuple')
    args=values[pos:pos+arity[0]];pos+=arity[0]
    if target not in selected:continue
    row={'consumer':n,'property':name,'provider':target,'args':args}
    refs.append(row)
    if len(args)!=2 or not 1<=args[0]<=selected[target]['pin_count']:errors.append(row)
 for n,props in tree.items():
  if not active(n):continue
  owner=parent(n)
  while owner and owner not in selected:owner=parent(owner)
  if owner not in selected:continue
  for field in ['pins','groups']:
   if field not in props:continue
   names=props[field].rstrip(b'\0').decode().split('\0')
   for pin_name in names:
    row={'node':n,'property':field,'provider':owner,'pin':pin_name}
    pin_configs.append(row)
    match=re.fullmatch(selected[owner]['kind']+r'(\d+)',pin_name)
    if not match or not 1<=int(match.group(1))<=selected[owner]['pin_count']:errors.append(row)
 report['inputs'][key]={'providers':selected,'gpio_provider_count':sum(p['kind']=='gpio' for p in selected.values()),'mpp_provider_count':sum(p['kind']=='mpp' for p in selected.values()),'references_checked':len(refs),'references':refs,'pin_config_mentions':len(pin_configs),'pin_configs':pin_configs,'invalid_pin_references':errors,'unparsed_gpio_properties':sorted(set(unparsed))}
report['passed']=not any(r['invalid_pin_references'] for r in report['inputs'].values())
(ROOT/'reports/pmic-pinctrl-dt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:{n:v for n,v in r.items() if n not in ('providers','references','pin_configs','unparsed_gpio_properties')} for k,r in report['inputs'].items()},indent=2))
raise SystemExit(not report['passed'])
