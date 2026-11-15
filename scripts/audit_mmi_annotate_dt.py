#!/usr/bin/env python3
"""Check selected stock/candidate annotate reservation and preserved ARM64 header declarations."""
from pathlib import Path
import hashlib
import json
import re
import struct
import subprocess
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
PIN='a24357771fe451d829a2f78f8986ecd53522085e'
original=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/vendor-repos/motorola-kernel-modules'),'show',PIN+':drivers/mmi_annotate/mmi_annotate.c'])
staged=(ROOT/'ports/moto-info/mmi_annotate/mmi_annotate.c').read_bytes()
def header(raw):
    return ' '.join(re.search(r'struct persist_data_t\s*\{(.*?)\};',raw.decode(),re.S).group(1).split())
assert header(original)==header(staged)
def cells(raw):return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
inputs={}
for label,file in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
    raw=(ROOT/file).read_bytes();tree,_=fdt(raw)
    refs={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
    matches=[n for n,p in tree.items() if p.get('compatible')==b'mmi,annotate\0'];assert len(matches)==1
    n=matches[0];p=tree[n];region=refs[cells(p['memory-region'])[0]];r=tree[region]
    reg=cells(r['reg']);base=reg[0]<<32|reg[1];size=reg[2]<<32|reg[3]
    assert region.startswith('/reserved-memory/') and 'no-map' in r
    assert p.get('status',b'okay\0') in [b'okay\0',b'ok\0'] and base%8==0 and size==4096
    assert cells(p['mem-size'])==[8192] and 'persist_unsupported' not in p
    for parent,props in tree.items():
        if n.startswith(parent+'/'):assert props.get('status',b'okay\0') in [b'okay\0',b'ok\0']
    inputs[label]={'file':file,'sha256':hashlib.sha256(raw).hexdigest(),'node':n,'reservation':region,'base':base,'size':size,'mem_size':8192,'no_map':True,'fallback':False}
assert all(inputs['saved'][k]==inputs['candidate'][k] for k in ['node','reservation','base','size','mem_size','no_map','fallback'])
r={'scope':'selected DT inputs/header declarations only; not physical annotation/minidump backend or whole-board acceptance','passed':True,'factory_commit':PIN,'persistent_header_declarations_unchanged':header(staged),'inputs':inputs,'native_qemu_geometry':'same 4096-byte no-map reservation and 8192-byte local buffer; guest physical address differs','note':'Stock local buffer exceeds reservation size, so generic small-buffer truncation case is a robustness test, not proof that stock boot logs overran it.','physical_verified':False}
(ROOT/'reports/mmi-annotate-stage56-dt.json').write_text(json.dumps(r,indent=2)+'\n')
print('Verified no-map 4KiB reservation, 8KiB local buffer and preserved header fields')
