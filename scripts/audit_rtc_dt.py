#!/usr/bin/env python3
"""Compare pinned PMIC RTC registers with saved/candidate DT; no PMIC access."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
PIN='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
vendor=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/qcom-soc'),'show',PIN+':drivers/rtc/rtc-pm8xxx.c'])
native=(ROOT/'ports/qcom-rtc/rtc-pm8xxx.c').read_bytes()
def regs(raw):
    body=re.search(r'static const struct pm8xxx_rtc_regs pmk8350_regs = \{(.*?)\n\};',raw.decode(),re.S).group(1)
    out={}
    for name,value in re.findall(r'\.(\w+)\s*=\s*(0x[0-9a-f]+|BIT\(\d+\))',body):
        out[name]=1<<int(value[4:-1]) if value.startswith('BIT') else int(value,0)
    if len(out)!=7:raise ValueError('incomplete RTC register table')
    return out
expected=regs(vendor);actual=regs(native)
inputs={}
for label,file in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]:
    raw=(ROOT/file).read_bytes();tree,_=fdt(raw)
    matches=[(n,p) for n,p in tree.items() if p.get('compatible')==b'qcom,pmk8350-rtc\0']
    if len(matches)!=1:raise ValueError('expected exactly one PMK8350 RTC')
    n,p=matches[0]
    selected={k:v.hex() for k,v in p.items() if k!='phandle'}
    ancestors={name:tree[name].get('status',b'okay\0').rstrip(b'\0').decode() for name in tree if n.startswith(name+'/')}
    if any(v not in ['ok','okay'] for v in ancestors.values()):raise ValueError('disabled RTC ancestor')
    if p.get('status',b'okay\0') not in [b'okay\0',b'ok\0']:raise ValueError('disabled RTC')
    if p['reg']!=struct.pack('>II',0x6100,0x6200) or p['reg-names']!=b'rtc\0alarm\0':raise ValueError('unexpected RTC bases')
    if p['interrupts']!=struct.pack('>IIII',0,0x62,1,1):raise ValueError('unexpected RTC alarm IRQ')
    inputs[label]={'file':file,'sha256':hashlib.sha256(raw).hexdigest(),'node':n,'properties_except_phandle':selected,'ancestor_status':ancestors,'allow_set_time':'allow-set-time' in p,'nvmem_offset_present':'nvmem-cells' in p,'uefi_offset_present':'qcom,uefi-rtc-info' in p}
passed=actual==expected and inputs['saved']['properties_except_phandle']==inputs['candidate']['properties_except_phandle']
report={'scope':'selected RTC node, ancestor status and native/factory register table only; not whole-DT/SPMI/IRQ/probe/physical wake proof','passed':passed,'vendor_commit':PIN,'vendor_source_sha256':hashlib.sha256(vendor).hexdigest(),'native_source_sha256':hashlib.sha256(native).hexdigest(),'vendor_registers':expected,'native_registers':actual,'inputs':inputs,'physical_verified':False,'known_limits':['No DT time-write permission or kernel offset storage; set_time returns ENODEV like stock','Native optional UEFI and small-drift offset policy differs from factory, not selected by AVR DT','Physical SPMI timing, counter progression, alarm IRQ delivery and suspend/power-off wake untested','RTC module must be available in actual early module ordering; no assembled boot image yet']}
(ROOT/'reports/rtc-stage49-dt.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({'passed':passed,'registers_checked':len(actual),'rtc_nodes_per_tree':1,'physical_verified':False}))
raise SystemExit(not passed)
