#!/usr/bin/env python3
"""Check saved/candidate ADC channel table coverage without reading any PMIC."""
import hashlib
import json
from pathlib import Path
import re
import struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'ports/qcom-adc-thermal/qcom-spmi-adc5-gen3.c').read_text()
header=(ROOT/'ports/qcom-adc-thermal/include/dt-bindings/iio/qcom,spmi-vadc.h').read_text()
definitions={n:int(v,0) for n,v in re.findall(r'^#define\s+(\w+)\s+(0x[0-9a-fA-F]+|[0-9]+)\s*(?:/\*.*)?$',header,re.M)}
tables={}
for generation,table in [(3,'adc5_chans_pmic'),(4,'adc5_gen4_chans_pmic')]:
 body=re.search(r'static const struct adc5_channels '+table+r'\[ADC5_MAX_CHANNEL\] = \{(.*?)\n\};',source,re.S).group(1)
 names=re.findall(r'\[(\w+)\]\s*=\s*ADC5_CHAN_',body)
 tables[generation]={definitions[n]:n for n in names}
def cells(raw):
 if len(raw)%4:raise ValueError('Unaligned DT cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
report={'scope':'active provider, channel-table, basic parameter and qualified IIO specifier coverage only; no PMIC transaction/calibration/overheat proof','physical_verified':False,'inputs':{}}
for kind,path in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]:
 raw=(ROOT/path).read_bytes();tree,_=fdt(raw)
 def active(node):
  while node:
   if tree[node].get('status',b'okay\0').rstrip(b'\0') not in (b'ok',b'okay'):return False
   node=(node.rsplit('/',1)[0] or '/') if node!='/' else None
  return True
 providers={};alarms=[];errors=[]
 for node,props in tree.items():
  if not active(node):continue
  comp=strings(props.get('compatible',b''))
  if 'qcom,spmi-temp-alarm' in comp:alarms.append(node)
  parentgen=next((gen for gen in (3,4) if f'qcom,spmi-adc5-gen{gen}' in comp),None)
  if parentgen is None:continue
  channels=[]
  for child,p in tree.items():
   if child.rsplit('/',1)[0]!=node or not active(child):continue
   regs=cells(p.get('reg',b''))
   if len(regs)!=1:errors.append({'node':child,'error':'channel needs one reg cell'});continue
   virtual=regs[0];gen=4 if 'qcom,adc5-gen4' in p else parentgen
   code=virtual & 255
   row={'node':child,'virtual_channel':virtual,'generation':gen,'sid':(virtual>>8)&31,'bus':(virtual>>13)&3,'channel':code,'table_entry':tables[gen].get(code),'thermal_monitor_type':cells(p.get('qcom,adc-tm-type',b'\0'*4))[0]}
   channels.append(row)
   if virtual & ~0x7fff or code not in tables[gen]:errors.append({'node':child,'error':'unsupported virtual channel/table entry'})
   for field,allowed in [('qcom,decimation',[85,340,1360]),('qcom,avg-samples',[1,2,4,8,16]),('qcom,adc-tm-type',[0,1,2,3])]:
    if field in p and (len(cells(p[field]))!=1 or cells(p[field])[0] not in allowed):errors.append({'node':child,'error':'unsupported '+field})
  registers=cells(props.get('reg',b''))
  max_sdam_offset=int(re.search(r'^#define ADC5_GEN3_CONV_REQ\s+(0x[0-9a-f]+)',source,re.M).group(1),0)
  if not registers or any(r>0xffff-max_sdam_offset for r in registers):errors.append({'node':node,'error':'invalid SDAM register bases'})
  providers[node]={'parent_generation':parentgen,'sdam_bases':registers,'channels':channels}
 ph={cells(p['phandle'])[0]:node for node,p in tree.items() if 'phandle' in p}
 refs=[];unparsed=[]
 for node,p in tree.items():
  if not active(node) or 'io-channels' not in p:continue
  values=cells(p['io-channels']);pos=0
  while pos<len(values):
   ident=values[pos];pos+=1;target=ph.get(ident)
   if target is None or '#io-channel-cells' not in tree[target]:unparsed.append(node);break
   arity=cells(tree[target]['#io-channel-cells'])
   if len(arity)!=1 or pos+arity[0]>len(values):raise ValueError('Malformed IIO specifier')
   args=values[pos:pos+arity[0]];pos+=arity[0]
   if target not in providers:continue
   row={'consumer':node,'provider':target,'args':args};refs.append(row)
   if len(args)!=1 or args[0] not in [c['virtual_channel'] for c in providers[target]['channels']]:errors.append({'node':node,'error':'unknown ADC IIO channel','args':args})
 channels=[c for p in providers.values() for c in p['channels']]
 report['inputs'][kind]={'sha256':hashlib.sha256(raw).hexdigest(),'provider_count':len(providers),'channel_count':len(channels),'gen3_channel_count':sum(c['generation']==3 for c in channels),'gen4_channel_count':sum(c['generation']==4 for c in channels),'thermal_alarm_count':len(alarms),'iio_references_checked':len(refs),'providers':providers,'thermal_alarms':alarms,'iio_references':refs,'unparsed_iio_consumers':unparsed,'errors':errors}
report['passed']=not any(row['errors'] for row in report['inputs'].values())
(ROOT/'reports/adc-thermal-dt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({kind:{k:v for k,v in row.items() if k.endswith('_count') or k in ('errors','iio_references_checked')} for kind,row in report['inputs'].items()},indent=2))
raise SystemExit(not report['passed'])
