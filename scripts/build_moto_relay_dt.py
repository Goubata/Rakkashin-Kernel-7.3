#!/usr/bin/env python3
"""Add the factory relay broker binding to a new file; never write device DT."""
from pathlib import Path
import hashlib
import json
import struct
from audit_boot_tree import fdt
from build_dt_components import run,inside,DTC
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'artifacts/dt-spmi-stage19/avr-spmi-candidate.dtb'
source=ROOT/'ports/device-tree/moto/avr-mmi-relay.dtso'
out=ROOT/'artifacts/dt-moto-relay-stage53';out.mkdir(parents=True,exist_ok=True)
raw=base.read_bytes();assert hashlib.sha256(raw).hexdigest()=='3bbb2c9faaad8777ea0a9aabd0f603e780aa263c9a37766217da7925f6df750d'
before,_=fdt(raw);target='/soc/mmi-relay'
assert target not in before and before['/soc']['compatible']==b'simple-bus\0'
assert before['/soc'].get('status',b'okay\0') in [b'ok\0',b'okay\0']
overlay=out/'avr-mmi-relay.dtbo';candidate=out/'avr-moto-relay-candidate.dtb'
run([DTC+'dtc','-@','-I','dts','-O','dtb','-o',inside(overlay),inside(source)],out/'compile.log')
run([DTC+'fdtoverlay','-i',inside(base),'-o',inside(candidate),inside(overlay)],out/'apply.log')
result=candidate.read_bytes();after,_=fdt(result)
assert set(after)-set(before)=={target} and not set(before)-set(after)
assert all(after[n]==p for n,p in before.items())
assert [n for n in after if n in before]==list(before),'pre-existing node order changed'
assert after[target]=={'compatible':b'mmi,sys-relay\0','status':b'okay\0'}
def reservations(data):
    offset=struct.unpack_from('>I',data,16)[0];values=[]
    while True:
        pair=struct.unpack_from('>QQ',data,offset);offset+=16
        if pair==(0,0):return values
        values.append(pair)
assert reservations(raw)==reservations(result)
GPU='/soc/qcom,kgsl-3d0@3d00000/qcom,gpu-pwrlevel-bins/qcom,gpu-pwrlevels-0'
levels=[n for n in after if n.rsplit('/',1)[0]==GPU];assert len(levels)==21
r={'scope':'add one source-defined software relay provider binding only; not physical/whole-board DT acceptance','passed':True,'base_file':str(base.relative_to(ROOT)),'base_sha256':hashlib.sha256(raw).hexdigest(),'overlay_source':str(source.relative_to(ROOT)),'overlay_source_sha256':hashlib.sha256(source.read_bytes()).hexdigest(),'candidate_file':str(candidate.relative_to(ROOT)),'candidate_sha256':hashlib.sha256(result).hexdigest(),'added_node':target,'binding':'mmi,sys-relay','preexisting_node_properties_and_order_unchanged':True,'reservation_map_unchanged':True,'saved_gpu_levels_unchanged':21,'physical_verified':False,'partition_written':False,'whole_board_dt_accepted':False,'reason':'Neither saved nor running AVR tree contains the factory relay compatible; loaded stock relay driver has no bound device. Candidate provides a broker instance instead of leaving all APIs uninitialized. Real framework probe/API behavior is tested separately on synthetic platform devices.','pending':['Actual OF platform population with this candidate and ROM consumers','Whole-board SID4 conflict and all existing DT/boot acceptance gaps']}
(ROOT/'reports/moto-touch-deps-stage53-dt.json').write_text(json.dumps(r,indent=2)+'\n')
print('Added one relay node; existing DT properties/order/reservations and all 21 GPU levels unchanged')
