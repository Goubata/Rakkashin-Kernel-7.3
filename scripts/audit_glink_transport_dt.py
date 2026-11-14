#!/usr/bin/env python3
"""Check active GLINK edge references and selected pinned wire constants only."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
PIN='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
repo=ROOT.parent/'avr-upstream-audit/qcom-soc'
vendor_smem=subprocess.check_output(['git','-C',str(repo),'show',PIN+':drivers/rpmsg/qcom_glink_smem.c']).decode()
vendor_core=subprocess.check_output(['git','-C',str(repo),'show',PIN+':drivers/rpmsg/qcom_glink_native.c']).decode()
smem=(ROOT/'ports/qcom-glink-transport/qcom_glink_smem.c').read_text()
core=(ROOT/'ports/qcom-glink-transport/qcom_glink_native.c').read_text()
smem_provider=(ROOT/'ports/qcom-smem/smem.c').read_text()
host_count=int(re.search(r'^#define SMEM_HOST_COUNT\s+(\d+)',smem_provider,re.M).group(1))
def numeric_constants(text,prefix):
 return {k:int(v,0) for k,v in re.findall(r'^#define\s+('+prefix+r'\w*)\s+(0x[0-9a-fA-F]+|[0-9]+)\s*$',text,re.M)}
wire_native=numeric_constants(core,'GLINK_CMD_');wire_vendor=numeric_constants(vendor_core,'GLINK_CMD_')
wire_mismatches={k:{'native':v,'vendor':wire_vendor[k]} for k,v in wire_native.items() if k in wire_vendor and v!=wire_vendor[k]}
ids_native=numeric_constants(smem,'SMEM_GLINK_NATIVE_XPRT_');ids_vendor=numeric_constants(vendor_smem,'SMEM_GLINK_NATIVE_XPRT_')
report={'scope':'active DT edge cells, saved property equality and selected wire command/SMEM IDs only; not firmware, mailbox/IRQ delivery, partition presence or protocol equivalence proof','vendor_source_commit':PIN,'native_smem_ids':ids_native,'vendor_smem_ids':ids_vendor,'common_wire_commands_checked':len(wire_native.keys() & wire_vendor.keys()),'wire_command_mismatches':wire_mismatches,'vendor_only_commands':sorted(wire_vendor.keys()-wire_native.keys()),'native_advertised_features':['GLINK_FEATURE_INTENT_REUSE'], 'vendor_additional_advertised_features':['GLINK_FEATURE_ZERO_COPY'], 'zero_copy_implemented':False,'wire_subset_only':True,'physical_verified':False,'inputs':{}}
for kind,name in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw)
 def active(node):
  while node:
   if tree[node].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   node=(node.rsplit('/',1)[0] or '/') if node!='/' else None
  return True
 def cells(raw):
  if len(raw)%4:raise ValueError('Unaligned cell property')
  return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
 ph={cells(p['phandle'])[0]:node for node,p in tree.items() if 'phandle' in p}
 edges=[];errors=[]
 for node,p in tree.items():
  if node.rsplit('/',1)[-1]!='glink-edge' or not active(node):continue
  hosts=cells(p.get('qcom,remote-pid',b''));irq_parent=cells(p.get('interrupt-parent',b''));mbox=cells(p.get('mboxes',b''))
  mailbox=ph.get(mbox[0]) if mbox else None
  arity=cells(tree[mailbox].get('#mbox-cells',b'')) if mailbox else []
  row={'node':node,'label':p.get('label',b'').rstrip(b'\0').decode(),'remote_pid':hosts,'interrupt_parent':ph.get(irq_parent[0]) if len(irq_parent)==1 else None,'interrupts':cells(p.get('interrupts',b'')),'mailbox_provider':mailbox,'mailbox_cells':mbox,'mailbox_args':mbox[1:],'mailbox_arity':arity,'transport':p.get('transport',b'').rstrip(b'\0').decode()}
  edges.append(row)
  if len(hosts)!=1 or not 0<hosts[0]<host_count:errors.append({'node':node,'error':'host outside staged SMEM provider range'})
  if row['interrupt_parent'] is None or not row['interrupts']:errors.append({'node':node,'error':'missing IRQ provider/specifier'})
  if mailbox is None or len(arity)!=1 or len(mbox)!=arity[0]+1:errors.append({'node':node,'error':'malformed or unknown mailbox tuple'})
 report['inputs'][kind]={'sha256':hashlib.sha256(raw).hexdigest(),'edges':edges,'active_edge_count':len(edges),'errors':errors}
report['smem_host_count']=host_count
normalize=lambda rows:[{k:v for k,v in row.items() if k!='mailbox_cells'} for row in rows]
report['selected_edge_semantics_equal']=normalize(report['inputs']['saved']['edges'])==normalize(report['inputs']['candidate']['edges'])
report['raw_mailbox_phandle_renumbered']=any(a['mailbox_cells'][:1]!=b['mailbox_cells'][:1] for a,b in zip(report['inputs']['saved']['edges'],report['inputs']['candidate']['edges']))
report['passed']=not wire_mismatches and ids_native==ids_vendor and report['selected_edge_semantics_equal'] and not any(r['errors'] for r in report['inputs'].values())
(ROOT/'reports/glink-transport-dt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='inputs'},indent=2))
print(json.dumps({kind:{'active_edge_count':r['active_edge_count'],'errors':r['errors']} for kind,r in report['inputs'].items()},indent=2))
raise SystemExit(not report['passed'])
