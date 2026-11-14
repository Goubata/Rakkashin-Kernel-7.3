#!/usr/bin/env python3
"""Check saved GLINK channel coverage; no remote subsystem operation is inferred."""
import hashlib
import json
from pathlib import Path
import re
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'ports/qcom-pmic-glink/qti_pmic_glink.c').read_text()
body=re.search(r'pmic_glink_rpmsg_match\[\] = \{(.*?)\n\};',source,re.S).group(1)
channels=set(re.findall(r'"([^"]+)"',body))
native=(ROOT.parent/'avr-upstream-audit/android-7.3/drivers/remoteproc/qcom_q6v5_pas.c').read_text()
vendor=(ROOT.parent/'avr-upstream-audit/qcom-soc/drivers/remoteproc/qcom_q6v5_pas.c').read_text()
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
report={'scope':'selected DT strings/compatible coverage only; no firmware boot, IRQ, communication or charging proof','inputs':{},'physical_verified':False}
for kind,name in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw)
 def active(node):
  while node:
   if tree[node].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
   node=(node.rsplit('/',1)[0] or '/') if node!='/' else None
  return True
 providers=[];errors=[];remotes=[]
 for node,p in tree.items():
  if not active(node):continue
  compatible=strings(p.get('compatible',b''))
  if 'qcom,qti-pmic-glink' in compatible:
   channel=strings(p.get('qcom,pmic-glink-channel',b''))
   row={'node':node,'channels':channel,'subsystems':strings(p['qcom,subsys-name']) if 'qcom,subsys-name' in p else [],'protection_domain':strings(p['qcom,protection-domain']) if 'qcom,protection-domain' in p else [],'active_children':[{'node':n,'compatible':strings(v.get('compatible',b''))} for n,v in tree.items() if n.rsplit('/',1)[0]==node and active(n)],'inactive_children':[n for n in tree if n.rsplit('/',1)[0]==node and not active(n)]}
   providers.append(row)
   if len(channel)!=1 or channel[0] not in channels or len(channel[0].encode())>=32:errors.append({'node':node,'error':'unsupported or malformed RPMSG channel'})
   if row['protection_domain'] and (len(row['protection_domain'])!=2 or any(len(s.encode())>=64 for s in row['protection_domain'])):errors.append({'node':node,'error':'unsupported service/path string lengths'})
  if 'qcom,alor-soccp-pas' in compatible:
   remotes.append({'node':node,'compatible':'qcom,alor-soccp-pas','native_match_present':'"qcom,alor-soccp-pas"' in native,'vendor_match_present':'"qcom,alor-soccp-pas"' in vendor,'port_or_equivalent_hardware_implementation_required':True})
 names=[p['channels'][0] for p in providers if len(p['channels'])==1]
 if len(names)!=len(set(names)):errors.append({'error':'ambiguous duplicate provider channel'})
 report['inputs'][kind]={'sha256':hashlib.sha256(raw).hexdigest(),'provider_count':len(providers),'active_child_count':sum(len(p['active_children']) for p in providers),'providers':providers,'remoteproc_dependencies':remotes,'errors':errors}
report['selected_channel_validation_passed']=not any(r['errors'] for r in report['inputs'].values())
report['whole_board_ready']=False
(ROOT/'reports/pmic-glink-dt-validation.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:{n:r[n] for n in ['provider_count','active_child_count','remoteproc_dependencies','errors']} for k,r in report['inputs'].items()},indent=2))
raise SystemExit(not report['selected_channel_validation_passed'])
