#!/usr/bin/env python3
"""Record Alor PAS provider prerequisites from pinned source and saved DT; never boot firmware."""
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
parser.add_argument('--output',default='reports/soccp-provider-requirements-stage36.json')
args=parser.parse_args()
PIN='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
BASE='4dd306a07b2f456fd70ddaa28f4f65640374f202'
def source(repo,commit,name):
 return subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit'/repo),'show',commit+':'+name]).decode()
vendor=source('qcom-soc',PIN,'drivers/remoteproc/qcom_q6v5_pas.c')
native=source('android-7.3',BASE,'drivers/remoteproc/qcom_q6v5_pas.c')
helper=source('android-7.3',BASE,'drivers/remoteproc/qcom_q6v5.c')
def descriptor(text,name):
 raw=re.search(r'static const struct \w+ '+name+r' = \{(.*?)\n\};',text,re.S).group(1)
 result={}
 for k,v in re.findall(r'\.(\w+)\s*=\s*("[^"]*"|true|false|0x[0-9a-fA-F]+|[0-9]+)\s*,',raw):
  result[k]=json.loads(v) if v.startswith('"') or v in ['true','false'] else int(v,0)
 result['proxy_pd_names']=re.findall(r'"([^"]+)"',re.search(r'\.proxy_pd_names\s*=.*?\{(.*?)\}',raw,re.S).group(1)) if '.proxy_pd_names' in raw else []
 return result
alor=descriptor(vendor,'alor_soccp_resource')
comparisons={n:descriptor(native,n) for n in ['kaanapali_soccp_resource','glymur_soccp_resource']}
raw=(ROOT/'artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb').read_bytes();tree,_=fdt(raw)
cells=lambda v:list(struct.unpack('>'+str(len(v)//4)+'I',v))
ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
node='/soc/remoteproc-soccp@a3380000';props=tree[node]
rails=[]
for name in props['reg-names'].rstrip(b'\0').decode().split('\0'):
 provider=ph[cells(props[name+'-supply'])[0]];parent=provider.rsplit('/',1)[0]
 rails.append({'name':name,'provider':provider,'resource_parent':parent,
  'resource_compatible':tree[parent]['compatible'].rstrip(b'\0').decode(),
  'resource_name':tree[parent]['qcom,resource-name'].rstrip(b'\0').decode(),
  'raw_vote_and_load':cells(props[name+'-uV-uA']),
  'provider_min_raw':cells(tree[provider]['regulator-min-microvolt']),
  'provider_max_raw':cells(tree[provider]['regulator-max-microvolt']),
  'interpretation':'RPMh ARC level vote; raw DT names contain microvolt but do not convert vote 64 to physical 64 uV'})
exports=set()
for p in [ROOT/'out/native-gki/Module.symvers',*sorted((ROOT/'ports').glob('*/Module.symvers'))]:
 for line in p.read_text().splitlines():
  if line:exports.add(line.split()[1])
required=['qcom_q6v5_init','qcom_q6v5_deinit','qcom_add_sysmon_subdev','qcom_sysmon_shutdown_irq_state','qcom_add_glink_subdev','qcom_add_pdm_subdev','devm_qcom_pas_context_alloc','qcom_pas_is_available']
report={'scope':'source/DT/export prerequisites only; no firmware, MMIO, IRQ, rail or PAS execution',
 'vendor_source_commit':PIN,'native_source_base':BASE,
 'source_sha256':{'vendor_pas':hashlib.sha256(vendor.encode()).hexdigest(),'native_pas':hashlib.sha256(native.encode()).hexdigest(),'native_q6v5_common':hashlib.sha256(helper.encode()).hexdigest()},
 'candidate_dtb_sha256':hashlib.sha256(raw).hexdigest(),'node':node,'alor_descriptor':alor,'native_comparison_descriptors':comparisons,'rails':rails,
 'has_power_domains':'power-domains' in props,
 'native_pas_regulator_names':['cx','px'],'required_saved_regulator_names':['cx','mx'],
 'native_pas_coredump_default':'ELFCLASS32' if 'rproc_coredump_set_elf_info(rproc, ELFCLASS32, EM_NONE)' in native else 'review required',
 'alor_vendor_uses_elf64':alor.get('uses_elf64',False),
 'vendor_check_status':alor.get('check_status',False),
 'vendor_tcsr_call_guard_found':bool(re.search(r'if \(adsp->check_status\) \{\s*if \(rproc_find_status_register',vendor)),
 'has_tcsr_properties':{k:k in props for k in ['soccp-tcsr','soccp-spare']},
 'selected_real_export_presence':{n:n in exports for n in required},
 'physical_verified':False,'compatible_alias_sufficient':False,'ready_for_firmware_boot':False,
 'implementation_requirements':[
  'Stage native Q6V5 common and real sysmon/QMI service providers; force actual declarations and verify final ELF imports',
  'Use an Alor descriptor with pinned PAS/DTB PAS/firmware/crash/minidump values and a reviewed 64-bit dump path',
  'Implement counted regulator cx/mx ARC votes and rollback only completed requests; keep actual RPMh providers',
  'Do not attach kaanapali/glymur cx/mx generic power domains to this DT, which has no power-domains property',
  'Use native early-attach fatal/stop/ready line-state checks backed by the actual SMP2P provider; validate physical state',
  'Do not require or invent TCSR/spare properties merely because other vendor descriptors use them: Alor check_status is false',
  'Verify native PAS secure calls, firmware memory/resource-table/64-bit behavior and saved ROM ABI before any physical boot',
  'Keep all production helpers real; do not silently accept disabled sysmon or unavailable transport via header stubs']}
assert alor['pas_id']==51 and alor['dtb_pas_id']==65 and alor['minidump_id']==24
assert alor['uses_elf64'] and not report['vendor_check_status'] and report['vendor_tcsr_call_guard_found']
assert [r['resource_name'] for r in rails]==['cx.lvl','mx.lvl']
assert all(r['raw_vote_and_load']==[64,0] for r in rails) and not report['has_power_domains']
(ROOT/args.output).write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:report[k] for k in ['alor_descriptor','selected_real_export_presence','vendor_check_status','ready_for_firmware_boot']},indent=2))
