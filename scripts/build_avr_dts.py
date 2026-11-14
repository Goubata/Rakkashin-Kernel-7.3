#!/usr/bin/env python3
"""Compile pinned AVR source with the 7.3 DTC; no partition/image installation."""
from pathlib import Path
import argparse
import hashlib
import importlib.util
import json
import shlex
import subprocess

ROOT=Path(__file__).resolve().parents[1]
BUNDLE=ROOT/'artifacts/dt-source-bundle'
OUT=ROOT/'artifacts/dt-source-build'
ROOTFS=ROOT.parent/'.andronix/distros/ubuntu/rootfs'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--allow-partial',action='store_true',help='Retain incomplete research output; never authorizes packaging')
args=parser.parse_args()
lock=json.loads((ROOT/'reports/dt-source-lock.json').read_text())
if lock['status']!='include graph materialized':raise SystemExit('Prepare the pinned source graph first')
for item in lock['files']:
    path=BUNDLE/item['repository']/item['path']
    if hashlib.sha256(path.read_bytes()).hexdigest()!=item['sha256']:raise SystemExit('Source changed: '+str(path))
OUT.mkdir(parents=True,exist_ok=True)
def inside(path):return '/work/avr-port/'+path.relative_to(ROOT).as_posix()
def run(command,log):
    full=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin']+command
    with log.open('wb') as f:subprocess.run(['su','-c',shlex.join(full)],stdout=f,stderr=subprocess.STDOUT,check=True)
    return full
includes=[BUNDLE/'qcom-soc/include',BUNDLE/'kernel-common/include']
includes+=sorted({(BUNDLE/i['repository']/i['path']).parent for i in lock['files']})
commands={}
for name,source,suffix in [('base','avr-base.dts','dtb'),('overlay','avr-overlay.dtso','dtbo')]:
    prepared=OUT/(name+'.preprocessed.dts');result=OUT/('avr-'+name+'.'+suffix)
    cpp=['cc','-E','-P','-nostdinc','-undef','-D__DTS__','-x','assembler-with-cpp']
    for directory in includes:cpp+=['-I',inside(directory)]
    cpp += [inside(ROOT/'ports/device-tree'/source),'-o',inside(prepared)]
    commands[name+'_cpp']=run(cpp,OUT/(name+'-cpp.log'))
    dtc=['/work/avr-port/out/native-gki/scripts/dtc/dtc','-@','-H','epapr',
         '-I','dts','-O','dtb','-o',inside(result),inside(prepared)]
    commands[name+'_dtc']=run(dtc,OUT/(name+'-dtc.log'))
merged=OUT/'avr-stock-source-merged.dtb'
commands['overlay_apply']=run(['/work/avr-port/out/native-gki/scripts/dtc/fdtoverlay',
    '-i',inside(OUT/'avr-base.dtb'),'-o',inside(merged),inside(OUT/'avr-overlay.dtbo')],OUT/'overlay-apply.log')
spec=importlib.util.spec_from_file_location('dt_audit',ROOT/'scripts/audit_boot_tree.py')
audit=importlib.util.module_from_spec(spec);spec.loader.exec_module(audit)
comparisons={}
for name,rebuilt,stock in [('base',OUT/'avr-base.dtb',ROOT/'artifacts/dt-input-audit/vendor-base-0.dtb'),
                           ('overlay',OUT/'avr-overlay.dtbo',ROOT/'artifacts/dt-input-audit/overlay-0.dtbo'),
                           ('merged',merged,ROOT/'artifacts/dt-input-audit/stock-merged.dtb')]:
    left,_=audit.fdt(stock.read_bytes());right,_=audit.fdt(rebuilt.read_bytes())
    comparisons[name]={'stock':audit.summary(left),'rebuilt':audit.summary(right),
                       'raw_difference':audit.compare(left,right),
                       'limited_reference_normalization':audit.semantic_compare(left,right)}
missing={k:len(v['raw_difference']['left_only_nodes']) for k,v in comparisons.items()}
complete=not any(missing.values())
report={'hardware_graph_complete':complete,'missing_node_counts':missing,
        'packaging_allowed':False,'status':'partial research reconstruction' if not complete else 'node coverage only; binding and driver review pending',
        'scope':'7.3 toolchain source reconstruction of stock hardware description; not phone validation or a flashable boot/dtbo image',
        'source_files':len(lock['files']),'source_lock':'dt-source-lock.json','commands':commands,
        'sha256':{p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in [OUT/'avr-base.dtb',OUT/'avr-overlay.dtbo',merged]},
        'comparisons':comparisons}
(ROOT/'reports/dt-source-build.json').write_text(json.dumps(audit.public_report(report),indent=2)+'\n')
print(json.dumps({'source_files':len(lock['files']),'outputs':report['sha256'],
                  'node_counts':{k:v['rebuilt']['node_count'] for k,v in comparisons.items()}},indent=2))

if not complete and not args.allow_partial:
    raise SystemExit('Incomplete hardware graph; see reports/dt-source-build.json. Use --allow-partial only for research.')
