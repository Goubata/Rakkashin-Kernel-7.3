#!/usr/bin/env python3
"""Snapshot the partial port; this never creates or flashes a boot.img."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import subprocess

ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'out/native-gki'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--name',default='android73-boot-core-stage2')
parser.add_argument("--gpu-deps",action="store_true")
parser.add_argument("--rakkashin",action="store_true")
args=parser.parse_args()
if not args.name.replace('-','').replace('_','').isalnum():parser.error('invalid snapshot name')
dst=ROOT/'artifacts'/args.name
dst.mkdir(parents=True,exist_ok=True)
for path,name in [(out/'arch/arm64/boot/Image','Image'),(out/'.config','config'),(out/'Module.symvers','Module.symvers'),(out/'System.map','System.map')]:
    shutil.copyfile(path,dst/name)
modules=[]
ports=['qcom-clock-core','qcom-rpmh','qcom-clock-controllers','qcom-irq','pinctrl-alor-vendor-core','avr-oplus-sync']
if args.gpu_deps:
    ports += ['qcom-hwspinlock','qcom-smem','qcom-socinfo','qcom-llcc','qcom-dcvs',
              'governor-adreno-ro','governor-gpubw','governor-adreno-tz','qcom-iommu-util','secure-buffer','iommu-logger','arm-smmu-vendor','msm-sysstats']
if args.rakkashin:
    ports.append("rakkashin/adios")
    modules.append((out/"drivers/misc/ntsync.ko","modules/ntsync/ntsync.ko"))
for name in ports:
    modules += [(p,'modules/'+name+'/'+p.name) for p in (ROOT/'ports'/name).glob('*.ko')]
for name in ['qcom-scm','qcom_pas','qcom_tzmem']:
    p=out/'drivers/firmware/qcom'/(name+'.ko')
    modules.append((p,'modules/firmware/'+p.name))
for source,name in modules:
    target=dst/name
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,target)
report={
    'source_commit':subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/android-7.3'),'rev-parse','HEAD'],text=True).strip(),
    'description':'Partial port, no physical boot; KGSL not yet linked; not a flashable boot image',
    'module_count':len(modules),'modules':sorted(name for _,name in modules),
    'image_sha256':hashlib.sha256((dst/'Image').read_bytes()).hexdigest(),
}
(dst/'metadata.json').write_text(json.dumps(report,indent=2)+'\n')
files=sorted(p for p in dst.rglob('*') if p.is_file() and p.name!='SHA256SUMS')
(dst/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(dst).as_posix()+'\n' for p in files))
(ROOT/'reports'/(args.name+'.json')).write_text(json.dumps(report,indent=2)+'\n')
print('Staged',len(modules),'modules; Image',report['image_sha256'])
