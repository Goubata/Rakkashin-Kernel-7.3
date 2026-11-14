#!/usr/bin/env python3
"""Apply one evidence-backed candidate DT repair to files only; never flash."""
from pathlib import Path
import hashlib
import json
import subprocess
import sys
from audit_boot_tree import fdt
from build_dt_components import run, inside, DTC
ROOT=Path(__file__).resolve().parents[1]
subprocess.run([sys.executable,str(ROOT/'scripts/audit_saved_interconnect.py')],check=True)
base=ROOT/'artifacts/dt-avr-dump-aligned-build/avr-experimental-merged.dtb'
source=ROOT/'ports/device-tree/recovered/avr-usb-ipa-interconnect.dtso'
out=ROOT/'artifacts/dt-interconnect-stage18';out.mkdir(parents=True,exist_ok=True)
prepared=out/'avr-usb-ipa.preprocessed.dts';overlay=out/'avr-usb-ipa.dtbo';merged=out/'avr-interconnect-candidate.dtb'
run(['clang','-E','-P','-nostdinc','-undef','-D__DTS__','-x','assembler-with-cpp','-I',inside(ROOT/'ports/qcom-interconnect/include'),inside(source),'-o',inside(prepared)],out/'cpp.log')
run([DTC+'dtc','-@','-H','epapr','-I','dts','-O','dtb','-o',inside(overlay),inside(prepared)],out/'dtc.log')
run([DTC+'fdtoverlay','-i',inside(base),'-o',inside(merged),inside(overlay)],out/'apply.log')
a,_=fdt(base.read_bytes());b,_=fdt(merged.read_bytes())
if a.keys()!=b.keys():raise ValueError('Route repair changed node set')
changes=[{'node':n,'property':k} for n in a for k in a[n].keys()|b[n].keys() if a[n].get(k)!=b[n].get(k)]
if changes!=[{'node':'/soc/ssusb@a600000','property':'interconnects'}]:raise ValueError('Unexpected DT changes: '+str(changes))
for script in ['reconstruct_gpu_dt.py','reconstruct_memory_dt.py']:
 subprocess.run([sys.executable,str(ROOT/'scripts'/script),'--verify-merged',str(merged)],check=True)
r={'scope':'one candidate endpoint-provider repair only; not USB/IPA hardware acceptance','changes':changes,'node_count':len(b),'saved_gpu_and_memory_preserved':True,'hardware_verified':False,'packaging_allowed':False,'sha256':{str(p.relative_to(ROOT)):hashlib.sha256(p.read_bytes()).hexdigest() for p in [base,source,prepared,overlay,merged]}}
(ROOT/'reports/interconnect-dt-repair.json').write_text(json.dumps(r,indent=2)+'\n')
print('Built candidate DT; exactly one property changed; saved GPU/memory preserved')
