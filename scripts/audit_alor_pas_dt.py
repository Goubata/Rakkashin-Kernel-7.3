#!/usr/bin/env python3
"""Check Alor descriptor and selected saved/candidate rail/firmware inputs, not physical PAS operation."""
from pathlib import Path
import hashlib
import argparse
import json
import re
import struct
import subprocess
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--output',default='reports/pas-alor-stage39-dt-validation.json')
args=parser.parse_args()
PIN='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
source=(ROOT/'ports/qcom-pas-alor/qcom_q6v5_pas.c').read_text()
vendor=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/qcom-soc'),'show',PIN+':drivers/remoteproc/qcom_q6v5_pas.c']).decode()
def desc(text):
 raw=re.search(r'static const struct \w+ alor_soccp_resource = \{(.*?)\n\};',text,re.S).group(1)
 fields={}
 for k,v in re.findall(r'\.(\w+)\s*=\s*("[^"]*"|true|false|0x[0-9a-fA-F]+|[0-9]+)\s*,',raw):
  fields[k]=json.loads(v) if v.startswith('"') or v in ['true','false'] else int(v,0)
 return fields
native=desc(source);expected=desc(vendor)
fields=['crash_reason_smem','firmware_name','dtb_firmware_name','pas_id','dtb_pas_id','minidump_id','uses_elf64','ssr_name','sysmon_name','early_boot','auto_boot']
mismatches={n:{'staged':native.get(n),'vendor':expected.get(n)} for n in fields if native.get(n)!=expected.get(n)}
def cells(v):return list(struct.unpack('>'+str(len(v)//4)+'I',v))
def inspect(name):
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw);ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle'in p}
 n='/soc/remoteproc-soccp@a3380000';p=tree[n];names=p['reg-names'].rstrip(b'\0').decode().split('\0');rails=[]
 for name in names:
  target=ph[cells(p[name+'-supply'])[0]];parent=target.rsplit('/',1)[0]
  rails.append({'name':name,'provider':target,'compatible':tree[parent]['compatible'].rstrip(b'\0').decode(),'resource':tree[parent]['qcom,resource-name'].rstrip(b'\0').decode(),'request':cells(p[name+'-uV-uA'])})
 regions=[]
 for value in cells(p.get('memory-region',b'')):
  target=ph[value];regions.append({'node':target,'reg_cells':cells(tree[target]['reg'])})
 return {'sha256':hashlib.sha256(raw).hexdigest(),'selected':{'node':n,'compatible':p['compatible'].rstrip(b'\0').decode(),'reg_names':names,'rails':rails,'has_power_domains':'power-domains'in p,'memory_regions':regions,'memory_region_names':p.get('memory-region-names',b'').rstrip(b'\0').decode().split('\0') if p.get('memory-region-names') else [],'firmware_names':p.get('firmware-name',b'').rstrip(b'\0').decode().split('\0') if p.get('firmware-name') else []}}
inputs={k:inspect(n) for k,n in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]}
a,b=[v['selected'] for v in inputs.values()]
errors=[]
if mismatches:errors.append('pinned descriptor mismatch')
if a!=b:errors.append('selected saved/candidate semantics differ')
if b['reg_names']!=['cx','mx'] or b['has_power_domains']:errors.append('unexpected regulator/domain shape')
if b['compatible']!='qcom,alor-soccp-pas' or not native.get('arc_rails'):errors.append('Alor descriptor selection mismatch')
for row in b['rails']:
 if row['compatible']!='qcom,rpmh-arc-regulator' or row['request']!=[64,0]:errors.append('unexpected ARC request/provider')
report={'scope':'pinned descriptor fields and selected saved/candidate references only; not loader, probe, secure ABI or whole-board validation',
 'vendor_source_commit':PIN,'pas_source_sha256':hashlib.sha256(source.encode()).hexdigest(),'descriptor':native,'descriptor_fields_checked':fields,'descriptor_mismatches':mismatches,'inputs':inputs,
 'alor_compatible_present':bool(re.search(r'compatible = "qcom,alor-soccp-pas", .data = &alor_soccp_resource',source)),
 'physical_verified':False,'ready_for_firmware_boot':False,'errors':errors,'passed':not errors}
if not report['alor_compatible_present']:report['errors'].append('missing Alor binding');report['passed']=False
(ROOT/args.output).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':report['passed'],'descriptor_fields_checked':len(fields),'errors':errors,'memory_regions':len(b['memory_regions'])},indent=2))
raise SystemExit(not report['passed'])
