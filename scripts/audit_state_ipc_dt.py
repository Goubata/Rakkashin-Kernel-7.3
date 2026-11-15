#!/usr/bin/env python3
"""Validate selected saved/candidate IPCC/SMP2P references, not hardware delivery."""
from pathlib import Path
import hashlib
import json
import re
import subprocess
import struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0') if raw else []
def inspect(path):
 raw=path.read_bytes();tree,_=fdt(raw);errors=[]
 ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   n=(n.rsplit('/',1)[0] or '/') if n!='/' else None
  return True
 def refs(raw,arity):
  value=cells(raw);rows=[]
  while value:
   node=ph.get(value.pop(0))
   if node is None:raise ValueError('missing provider phandle')
   width=cells(tree[node][arity])
   if len(width)!=1 or len(value)<width[0]:raise ValueError('invalid provider arity')
   rows.append({'provider':node,'args':value[:width[0]]});value=value[width[0]:]
  return rows
 providers=[];edges=[]
 for n,p in tree.items():
  if not active(n):continue
  comp=strings(p.get('compatible',b''))
  if 'qcom,ipcc' in comp:
   row={'node':n,'reg':cells(p.get('reg',b'')),'interrupt_cells':cells(p.get('#interrupt-cells',b'')),'mailbox_cells':cells(p.get('#mbox-cells',b''))}
   providers.append(row)
   if row['interrupt_cells']!=[3] or row['mailbox_cells']!=[2]:errors.append(n+': IPCC arity')
  if 'qcom,smp2p' not in comp:continue
  try:
   irqparent=ph[cells(p['interrupt-parent'])[0]];irq=cells(p['interrupts']);mbox=refs(p['mboxes'],'#mbox-cells')
   local=cells(p['qcom,local-pid']);remote=cells(p['qcom,remote-pid']);ids=cells(p['qcom,smem'])
   entries=[]
   for cn,cp in tree.items():
    if cn.rsplit('/',1)[0]!=n or not active(cn):continue
    name=strings(cp['qcom,entry-name'])[0]
    incoming='interrupt-controller' in cp
    entries.append({'node':cn,'name':name,'direction':'inbound' if incoming else 'outbound',
                    'wire_name': name if incoming else name.encode()[:15].decode(),
                    'cells':cells(cp.get('#interrupt-cells' if incoming else '#qcom,smem-state-cells',b''))})
    if entries[-1]['cells']!=([2] if incoming else [1]):errors.append(cn+': entry arity')
   if len([e for e in entries if e['direction']=='outbound'])>16:errors.append(n+': outbound capacity')
   if len(local)!=1 or len(remote)!=1 or not 0<remote[0]<30 or local[0]!=0 or len(ids)!=2:errors.append(n+': SMEM identity')
   if 'qcom,ipcc' not in strings(tree[irqparent].get('compatible',b'')) or len(irq)!=3 or any(v>65535 for v in irq[:2]) or irq[:2]==[65535]*2:errors.append(n+': IPCC IRQ spec')
   if len(mbox)!=1 or mbox[0]['provider']!=irqparent or len(mbox[0]['args'])!=2 or any(v>65535 for v in mbox[0]['args']) or mbox[0]['args']==[65535]*2:errors.append(n+': IPCC mailbox spec')
   edges.append({'node':n,'local_pid':local,'remote_pid':remote,'smem_ids':ids,'irq_provider':irqparent,'irq':irq,'mailbox':mbox,'entries':entries})
  except (KeyError,ValueError,IndexError) as error:errors.append(n+': '+str(error))
 # Record the actual PAS client's state references in provider/bit form.
 n='/soc/remoteproc-soccp@a3380000';p=tree[n]
 try:
  states=refs(p.get('qcom,smem-states',b''),'#qcom,smem-state-cells')
  names=strings(p.get('qcom,smem-state-names',b''))
  if len(states)!=len(names):errors.append(n+': state-name count')
  for state in states:
   if not active(state['provider']) or len(state['args'])!=1 or state['args'][0]>=32:errors.append(n+': inactive/out-of-range state')
  client={'node':n,'states':states,'names':names}
 except (KeyError,ValueError) as error:errors.append(n+': '+str(error));client={}
 return {'sha256':hashlib.sha256(raw).hexdigest(),'ipcc_providers':sorted(providers,key=lambda r:r['node']),'smp2p_edges':sorted(edges,key=lambda r:r['node']),'soccp_client':client,'errors':errors}
report={'scope':'selected DT provider cells, identity ranges, outbound capacity and saved semantics only; not whole-board validation or firmware delivery',
        'hardware_verified':False,'inputs':{k:inspect(ROOT/p) for k,p in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]}}
pin='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
vendor=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/qcom-soc'),'show',pin+':drivers/soc/qcom/smp2p.c']).decode()
native=(ROOT/'ports/qcom-smp2p/smp2p.c').read_text()
header=lambda text:re.sub(r'\s+','',re.search(r'struct smp2p_smem_item \{.*?\} __packed;',text,re.S).group())
report['vendor_source_commit']=pin
report['shared_item_c_layout_equal']=header(native)==header(vendor)
report['wire_comparison_scope']='C shared-item layout and recorded native/vendor outbound strscpy(16) behavior only; not full firmware/IRQ/SSR equivalence'
report['vendor_long_outbound_names_preserved']=any(e['name']!=e['wire_name'] for row in report['inputs']['candidate']['smp2p_edges'] for e in row['entries'])
a,b=report['inputs'].values()
report['selected_semantics_equal']=all(a[k]==b[k] for k in ['ipcc_providers','smp2p_edges','soccp_client'])
report['passed']=report['shared_item_c_layout_equal'] and report['selected_semantics_equal'] and not a['errors'] and not b['errors']
(ROOT/'reports/state-ipc-dt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':report['passed'],'selected_semantics_equal':report['selected_semantics_equal'],
 'inputs':{k:{'ipcc_providers':len(v['ipcc_providers']),'smp2p_edges':len(v['smp2p_edges']),'errors':v['errors']} for k,v in report['inputs'].items()}},indent=2))
raise SystemExit(not report['passed'])
