#!/usr/bin/env python3
"""Match saved/candidate SDAM providers/cells and read-only live size metadata."""
import hashlib
import json
from pathlib import Path
import struct
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
def cells(raw):return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
live=json.loads((ROOT/'reports/sdam-stage50-live-readonly.json').read_text())
assert live['provider_count']==12 and not live['writes']
capacity={p['of_node'].removeprefix('/sys/firmware/devicetree/base'):p['data_bytes'] for p in live['providers'] if 'error' not in p}
inputs={}
for label,path in [('saved','artifacts/dt-input-audit/stock-merged.dtb'),('candidate','artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb')]:
    raw=(ROOT/path).read_bytes();tree,_=fdt(raw)
    refs={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p}
    rows=[];providers={n for n,p in tree.items() if p.get('compatible')==b'qcom,spmi-sdam\0'}
    for n in sorted(providers):
        p=tree[n];assert n in capacity and len(p['reg'])==4
        assert p.get('status',b'okay\0') in [b'okay\0',b'ok\0']
        for parent,properties in tree.items():
            if n.startswith(parent+'/'):assert properties.get('status',b'okay\0') in [b'okay\0',b'ok\0']
        children=[]
        for child,properties in tree.items():
            if child.startswith(n+'/'):
                off,length=cells(properties['reg'])
                assert length>0 and 0x40<=off<0x40+capacity[n] and length<=0x40+capacity[n]-off
                children.append({'node':child,'properties_except_phandle':{k:v.hex() for k,v in properties.items() if k!='phandle'}})
        consumers=[]
        for consumer,properties in tree.items():
            for prop in ['nvmem','nvmem-cells']:
                for ref in cells(properties.get(prop,b'')):
                    target=refs.get(ref)
                    if target==n or target and target.startswith(n+'/'):
                        consumers.append({'consumer':consumer,'property':prop,'target':target})
        rows.append({'node':n,'properties_except_phandle':{k:v.hex() for k,v in p.items() if k!='phandle'},'observed_data_bytes':capacity[n],'cells':sorted(children,key=lambda x:x['node']),'consumers':sorted(consumers,key=lambda x:(x['consumer'],x['property'],x['target']))})
    inputs[label]={'file':path,'sha256':hashlib.sha256(raw).hexdigest(),'providers':rows}
assert len(capacity)==12 and len(inputs['saved']['providers'])==12
passed=inputs['saved']['providers']==inputs['candidate']['providers']
r={'scope':'selected provider/cell geometry, resolved direct references and live stock capacity only; not consumer ABI/runtime/whole-DT/physical probe proof','passed':passed,'provider_count':12,'fixed_cell_count':sum(len(x['cells']) for x in inputs['saved']['providers']),'resolved_reference_count':sum(len(x['consumers']) for x in inputs['saved']['providers']),'inputs':inputs,'physical_candidate_verified':False,'pending':['Physical parent SPMI/IRQ/regmap readiness and probe','Actual haptics/LED/display/PON consumers not yet ported or accepted','Factory legacy bit versus bits cell spelling is preserved; only geometry audited','Complete module/ROM/SELinux/DT and boot artifacts']}
(ROOT/'reports/sdam-stage50-dt.json').write_text(json.dumps(r,indent=2)+'\n')
print(json.dumps({k:v for k,v in r.items() if k in ['passed','provider_count','fixed_cell_count','resolved_reference_count']}))
raise SystemExit(not passed)
