#!/usr/bin/env python3
"""Reconstruct saved static DTS with 7.3 DTC and require exact property preservation."""
from pathlib import Path
import hashlib
import importlib.util
import json
import shlex
import struct
import subprocess
ROOT=Path(__file__).resolve().parents[1]
OUT=ROOT/'artifacts/dt-dump-reconstruction'
INPUT=ROOT/'artifacts/dt-input-audit'
ROOTFS=ROOT.parent/'.andronix/distros/ubuntu/rootfs'
spec=importlib.util.spec_from_file_location('dt_audit',ROOT/'scripts/audit_boot_tree.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
OUT.mkdir(parents=True,exist_ok=True)
def inside(p):return '/work/avr-port/'+p.relative_to(ROOT).as_posix()
def run(args,log):
 command=['chroot',str(ROOTFS),'/work/avr-port/out/native-gki/scripts/dtc/'+args[0]]+args[1:]
 with log.open('wb') as f:subprocess.run(['su','-c',shlex.join(command)],stdout=f,stderr=subprocess.STDOUT,check=True)
 return command
def reservations(data):
 pos=struct.unpack_from('>I',data,16)[0];result=[]
 while pos+16<=len(data):
  address,size=struct.unpack_from('>QQ',data,pos);pos+=16
  if not address and not size:return result
  result.append([address,size])
 raise ValueError('unterminated reservation table')
rows=[];commands={}
for name,source,suffix in [('base','vendor-base-0.dtb','dtb'),('overlay','overlay-0.dtbo','dtbo')]:
 original=INPUT/source;dts=OUT/('avr-'+name+'.dts');output=OUT/('avr-'+name+'.'+suffix)
 # Keep generated source writable by the workspace owner even though DTC runs in chroot.
 if dts.exists():dts.unlink()
 dts.touch(mode=0o600)
 commands[name+'_decompile']=run(['dtc','-I','dtb','-O','dts','-o',inside(dts),inside(original)],OUT/(name+'-decompile.log'))
 if name=='overlay':
  text=dts.read_text()
  if '/plugin/;' not in text:text=text.replace('/dts-v1/;','/dts-v1/;\n/plugin/;',1)
  dts.write_text(text)
 cpuid=struct.unpack_from('>I',original.read_bytes(),28)[0]
 commands[name+'_compile']=run(['dtc','-@','-H','epapr','-b',str(cpuid),'-I','dts','-O','dtb','-o',inside(output),inside(dts)],OUT/(name+'-compile.log'))
 a=original.read_bytes();b=output.read_bytes();left,_=audit.fdt(a);right,_=audit.fdt(b)
 same=left==right and reservations(a)==reservations(b)
 rows.append({'name':name,'node_count':len(right),'all_properties_identical':left==right,
              'reservation_map_identical':reservations(a)==reservations(b),
              'input_sha256':hashlib.sha256(a).hexdigest(),'output_sha256':hashlib.sha256(b).hexdigest()})
 if not same:raise ValueError('DTC roundtrip changed static input: '+name)
merged=OUT/'avr-merged.dtb'
commands['overlay_apply']=run(['fdtoverlay','-i',inside(OUT/'avr-base.dtb'),'-o',inside(merged),inside(OUT/'avr-overlay.dtbo')],OUT/'apply.log')
a,_=audit.fdt((INPUT/'stock-merged.dtb').read_bytes());b,_=audit.fdt(merged.read_bytes())
if a!=b:raise ValueError('recompiled pair changed merged properties')
report={'scope':'exact static-input reconstruction using 7.3 DTC; not a phone boot or DT-schema result',
        'passed':True,'trees':rows,'merged_node_count':len(b),'merged_all_properties_identical':True,
        'runtime_metadata':'DRAM, bootargs, SCM and hypervisor updates remain bootloader-owned; runtime FDT is not baked into these files',
        'outputs':'artifacts/dt-dump-reconstruction','commands':commands,'flashable_image_created':False}
(ROOT/'reports/dt-dump-reconstruction.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps({k:v for k,v in report.items() if k!='commands'},indent=2))
