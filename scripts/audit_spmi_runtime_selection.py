#!/usr/bin/env python3
"""Read current SPMI sysfs selection only; never touch registers or bind/unbind."""
from pathlib import Path
import hashlib,json,shlex,struct,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
probe='''from pathlib import Path
import json,re,os
base=Path('/sys/bus/spmi/devices')
rows=[]
for p in sorted(base.iterdir()):
 if not re.fullmatch(r'[0-9]+-[0-9a-f]{2}',p.name):continue
 node=p/'of_node';driver=p/'driver'
 rows.append({'name':p.name,'node':str(node.resolve()).removeprefix('/sys/firmware/devicetree/base') if node.is_symlink() else None,'driver':os.readlink(driver).rsplit('/',1)[-1] if driver.is_symlink() else None})
print(json.dumps(rows))
'''
command=shlex.join([sys.executable,'-c',probe])
rows=json.loads(subprocess.check_output(['su','-c',command],text=True))
sys.path.insert(0,str(ROOT/'scripts'))
from audit_boot_tree import fdt
raw=(ROOT/'device-baseline/running.dtb').read_bytes();tree,_=fdt(raw)
parent='/soc/arbiter@c400000/spmi@c426000'
def enabled(n):
 while n:
  if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'okay',b'ok'):return False
  n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
 return True
first={};duplicates={}
for n,p in tree.items():
 if n.rsplit('/',1)[0]!=parent or not enabled(n):continue
 if b'qcom,spmi-pmic' not in p.get('compatible',b'').split(b'\0'):continue
 reg=p.get('reg',b'')
 if len(reg)!=8:continue
 sid,kind=struct.unpack('>II',reg)
 if kind:continue
 first.setdefault(sid,n);duplicates.setdefault(sid,[]).append(n)
main=[r for r in rows if r['node'] and r['node'].rsplit('/',1)[0]==parent]
for r in main:r['matches_saved_first']=first.get(int(r['name'].split('-')[1],16))==r['node']
report={'scope':'Read-only live sysfs enumeration/driver binding compared to saved DT order; no SPMI transactions, kernel switch or physical 7.3 validation','current_kernel':subprocess.check_output(['uname','-r'],text=True).strip(),'saved_dtb_sha256':hashlib.sha256(raw).hexdigest(),'devices':rows,'main_bus_devices':main,'all_present_main_nodes_match_saved_first':all(r['matches_saved_first'] for r in main) if main else False,'saved_active_duplicate_sids':{str(sid):nodes for sid,nodes in duplicates.items() if len(nodes)>1},'saved_first_nodes_not_observed':sorted(set(first.values())-{r['node'] for r in main}),'physical_73_verified':False,'dt_modified':False,'partition_written':False,'conclusion':'Observed SID4 currently enumerates pm8010@4 with no bound driver if driver is null. Do not disable or promote alternative pmd802x@4 without proving the required PMIC subtree and probe behavior.'}
(ROOT/'reports/spmi-runtime-stage64.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['current_kernel','all_present_main_nodes_match_saved_first','saved_active_duplicate_sids','saved_first_nodes_not_observed']},indent=2))
