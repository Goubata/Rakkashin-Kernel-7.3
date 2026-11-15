#!/usr/bin/env python3
"""Check selected PMIC key DT, native/factory descriptors and read-only stock wake defaults."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
PIN='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
native=(ROOT/'ports/qcom-power-key/pm8941-pwrkey.c').read_bytes()
vendor=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/qcom-soc'),'show',PIN+':drivers/input/misc/pm8941-pwrkey.c'])
def descriptor(raw,name):
    body=re.search(r'static const struct pm8941_data '+name+r' = \{(.*?)\n\};',raw.decode(),re.S).group(1)
    return dict(re.findall(r'\.(\w+)\s*=\s*("[^"]*"|\w+)',body))
descriptors={}
for name in ['pon_gen3_pwrkey_data','pon_gen3_resin_data']:
    n=descriptor(native,name);v=descriptor(vendor,name)
    assert {k:value for k,value in n.items() if k!='wakeup_source_default'}==v
    assert n['wakeup_source_default']=='true'
    descriptors[name]={'native':n,'vendor':v}
def cells(raw):return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
inputs={}
for label,file in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]:
    raw=(ROOT/file).read_bytes();tree,_=fdt(raw)
    parents=[n for n,p in tree.items() if p.get('compatible')==b'qcom,pm8998-pon\0']
    assert len(parents)==1;n=parents[0];p=tree[n]
    assert cells(p['reg'])==[0x1300,0x800] and p['reg-names']==b'pon_hlos\0pon_pbs\0'
    for parent,props in tree.items():
        if n.startswith(parent+'/'):assert props.get('status',b'okay\0') in [b'okay\0',b'ok\0']
    keys=[]
    for child,compatible,code,irq in [('pwrkey','qcom,pmk8350-pwrkey',116,7),('resin','qcom,pmk8350-resin',114,6)]:
        node=n+'/'+child;props=tree[node]
        assert props['compatible']==compatible.encode()+b'\0' and cells(props['linux,code'])==[code]
        assert cells(props['interrupts'])==[0,0x13,irq,3]
        assert props.get('status',b'okay\0') in [b'okay\0',b'ok\0']
        keys.append({'node':node,'properties_except_phandle':{k:v.hex() for k,v in props.items() if k!='phandle'}})
    inputs[label]={'file':file,'sha256':hashlib.sha256(raw).hexdigest(),'parent':n,'parent_properties_except_phandle':{k:v.hex() for k,v in p.items() if k!='phandle'},'keys':keys}
assert all(inputs['saved'][k]==inputs['candidate'][k] for k in ['parent','parent_properties_except_phandle','keys'])
live=json.loads((ROOT/'reports/power-key-stage51-live-readonly.json').read_text())
assert live['wakeup']=={'pmic_pwrkey':'enabled','pmic_resin':'enabled'}
r={'scope':'selected DT and descriptor fields plus stock sysfs wake defaults; no physical probe/IRQ/suspend or full ROM input proof','passed':True,'factory_source_commit':PIN,'factory_source_sha256':hashlib.sha256(vendor).hexdigest(),'native_source_sha256':hashlib.sha256(native).hexdigest(),'descriptors':descriptors,'inputs':inputs,'stock_wakeup':live['wakeup'],'candidate_physical_verified':False,'pending':['Actual SPMI/IRQ/probe and wake from suspend','Native PM suspend uses IRQ wake rather than vendor free/re-request sequence; hardware validation required','Other GPIO/touch/gesture/haptics and all ROM input consumers','Reboot/power-off actions and remaining boot artifacts']}
(ROOT/'reports/power-key-stage51-dt.json').write_text(json.dumps(r,indent=2)+'\n')
print('Verified two key descriptors, selected DT and stock wake defaults')
