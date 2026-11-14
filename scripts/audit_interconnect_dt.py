#!/usr/bin/env python3
"""Compare pinned Alor node tables with saved/candidate DT references, never probe."""
import argparse
import collections
import json
from pathlib import Path
import re
import struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__)
p.add_argument('--candidate',type=Path,default=ROOT/'artifacts/dt-interconnect-stage18/avr-interconnect-candidate.dtb')
a=p.parse_args()
source=(ROOT/'ports/qcom-interconnect/alor.c').read_text()
header=(ROOT/'ports/qcom-interconnect/include/dt-bindings/interconnect/qcom,alor.h').read_text()
ids={name:int(value,0) for name,value in re.findall(r'^#define\s+(\w+)\s+(0x[0-9a-fA-F]+|\d+)\s*$',header,re.M)}
node_blocks=dict(re.findall(r'static struct qcom_icc_node (\w+) = \{(.*?)\n\};',source,re.S))
arrays={name:{ids[key]:node for key,node in re.findall(r'\[(\w+)\]\s*=\s*&(\w+)',body)} for name,body in re.findall(r'static struct qcom_icc_node \*(\w+)\[\] = \{(.*?)\n\};',source,re.S)}
desc={name:re.search(r'\.nodes\s*=\s*(\w+)',body).group(1) for name,body in re.findall(r'static struct qcom_icc_desc (\w+) = \{(.*?)\n\};',source,re.S)}
compat={name:arrays[desc[data]] for name,data in re.findall(r'\.compatible\s*=\s*"([^"]+)"\s*,\s*\.data\s*=\s*&(\w+)',source)}
if len(compat)!=14 or not node_blocks or not ids:raise ValueError('Incomplete Alor table parser')
node_ids={ids[re.search(r'\.id\s*=\s*(\w+)',body).group(1)] for body in node_blocks.values()}
missing_links=[]
for name,body in node_blocks.items():
 links=re.search(r'\.links\s*=\s*\{([^}]+)\}',body)
 if links:
  for target in re.findall(r'\b(?:MASTER|SLAVE)_\w+',links.group(1)):
   if ids[target] not in node_ids:missing_links.append({'node':name,'target':target})
usage=collections.Counter()
for body in re.findall(r'static struct qcom_icc_bcm \w+ = \{(.*?)\n\};',source,re.S):
 match=re.search(r'\.nodes\s*=\s*\{([^}]+)\}',body)
 if match:usage.update(re.findall(r'&(\w+)',match.group(1)))
graph={ids[re.search(r'\.id\s*=\s*(\w+)',body).group(1)]:[ids[t] for t in re.findall(r'\b(?:MASTER|SLAVE)_\w+',re.search(r'\.links\s*=\s*\{([^}]+)\}',body).group(1))] if re.search(r'\.links\s*=\s*\{([^}]+)\}',body) else [] for body in node_blocks.values()}
queue=collections.deque([[ids['MASTER_USB3']]]);seen=set();route=None
while queue:
 path=queue.popleft();node=path[-1]
 if node==ids['SLAVE_IPA_CFG']:route=path;break
 if node in seen:continue
 seen.add(node)
 queue.extend(path+[target] for target in graph.get(node,[]))
if route is None:raise ValueError('Alor topology has no USB-to-IPA path')
report={'scope':'static node IDs, binding indices and reference encoding only; not route discovery, probe, votes or hardware','supported_provider_compatibles':sorted(compat),'usb_ipa_topology_path_ids':route,'source_nodes':len(node_blocks),'source_ids':len(node_ids),'unresolved_source_links':missing_links,'max_bcms_per_node':max(usage.values()),'node_bcm_capacity_exceeded':[n for n,c in usage.items() if c>3],'inputs':{},'packaging_allowed':False}
for name,relative in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate',a.candidate)]:
 tree,_=fdt((ROOT/relative).read_bytes())
 def cells(raw):
  if len(raw)%4:raise ValueError('Unaligned property')
  return struct.unpack('>'+str(len(raw)//4)+'I',raw)
 ph={cells(props['phandle'])[0]:path for path,props in tree.items() if 'phandle' in props}
 providers={}
 for path,props in tree.items():
  for value in props.get('compatible',b'').rstrip(b'\0').decode().split('\0'):
   if value in compat:providers[path]=value;break
 results=[];errors=[];unparsed=[]
 for path,props in tree.items():
  if 'interconnects' not in props:continue
  values=cells(props['interconnects']);pos=0
  while pos<len(values):
   target=ph.get(values[pos]);pos+=1
   if target is None or '#interconnect-cells' not in tree[target]:
    unparsed.append(path);break
   arity=cells(tree[target]['#interconnect-cells'])
   if len(arity)!=1 or pos+arity[0]>len(values):raise ValueError('Invalid interconnect reference')
   args=values[pos:pos+arity[0]];pos+=arity[0]
   if target in providers:
    table=compat[providers[target]]
    if not args or args[0] not in table:errors.append({'consumer':path,'provider':target,'args':args})
    else:results.append({'consumer':path,'provider':target,'id':args[0],'node':table[args[0]],'tag':args[1] if len(args)>1 else None})
 report['inputs'][name]={'providers':providers,'provider_count':len(providers),'references_checked':len(results),'invalid_node_references':errors,'unparsed_properties':sorted(set(unparsed)),'references':results}
report['saved_bindings_passed']=not report['inputs']['saved']['invalid_node_references']
report['candidate_bindings_passed']=not (report['inputs']['candidate']['invalid_node_references'] or report['inputs']['candidate']['unparsed_properties'])
report['passed']=not (missing_links or report['node_bcm_capacity_exceeded']) and report['candidate_bindings_passed']
(ROOT/'reports/interconnect-dt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='inputs'}))
print(json.dumps({n:{k:v for k,v in r.items() if k not in ('providers','references')} for n,r in report['inputs'].items()}))
raise SystemExit(not report['passed'])
