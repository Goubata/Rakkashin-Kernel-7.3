#!/usr/bin/env python3
"""Snapshot the partial port; this never creates or flashes a boot.img."""
from pathlib import Path
from native_module_inventory import firmware_modules, pmic_ipc_modules
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
parser.add_argument("--gpu-driver",action="store_true")
parser.add_argument("--ipc-deps",action="store_true")
parser.add_argument("--rakkashin",action="store_true")
parser.add_argument('--storage-phy',action='store_true')
parser.add_argument('--ice',action='store_true')
parser.add_argument('--ufs-host',action='store_true')
parser.add_argument('--regulators',action='store_true')
parser.add_argument('--interconnect',action='store_true')
parser.add_argument('--spmi',action='store_true')
parser.add_argument('--pmic-pinctrl',action='store_true')
parser.add_argument('--adc-thermal',action='store_true')
parser.add_argument('--pmic-ipc',action='store_true')
parser.add_argument('--pmic-glink',action='store_true')
parser.add_argument('--moto-touch-deps',action='store_true')
parser.add_argument('--panel-event',action='store_true')
parser.add_argument('--power-key',action='store_true')
parser.add_argument('--sdam',action='store_true')
parser.add_argument('--rtc',action='store_true')
args=parser.parse_args()
if args.moto_touch_deps:
    subprocess.run(['python',str(ROOT/'scripts/audit_moto_touch_deps_sources.py')],check=True)
    subprocess.run(['python',str(ROOT/'scripts/build_moto_relay_dt.py')],check=True)
    args.spmi=True
if args.panel_event:
    subprocess.run(['python',str(ROOT/'scripts/audit_panel_event_sources.py')],check=True)
if args.power_key:
    subprocess.run(['python',str(ROOT/'scripts/audit_power_key_sources.py')],check=True)
    args.spmi=True
if args.sdam:
    subprocess.run(['python',str(ROOT/'scripts/audit_sdam_sources.py')],check=True)
    args.spmi=True
if args.rtc:
    subprocess.run(['python',str(ROOT/'scripts/audit_rtc_sources.py')],check=True)
    args.spmi=True
if args.pmic_glink:args.pmic_ipc=True;args.ipc_deps=True
if args.pmic_ipc:args.adc_thermal=True
if args.adc_thermal:args.pmic_pinctrl=True
if args.pmic_pinctrl:args.spmi=True
if args.spmi:args.interconnect=True
if args.interconnect:args.regulators=True
if args.regulators:args.ufs_host=True
if args.ufs_host:args.ice=True;args.gpu_deps=True
if args.ice:args.storage_phy=True
if args.gpu_driver:
    args.gpu_deps = True
    args.ipc_deps = True
if not args.name.replace('-','').replace('_','').isalnum():parser.error('invalid snapshot name')
dst=ROOT/'artifacts'/args.name
dst.mkdir(parents=True,exist_ok=True)
for path,name in [(out/'arch/arm64/boot/Image','Image'),(out/'.config','config'),(out/'Module.symvers','Module.symvers'),(out/'System.map','System.map')]:
    shutil.copyfile(path,dst/name)
if args.moto_touch_deps:
    shutil.copyfile(ROOT/'artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb',dst/'candidate.dtb')
modules=[]
ports=['qcom-clock-core','qcom-rpmh','qcom-clock-controllers','qcom-irq','pinctrl-alor-vendor-core','avr-oplus-sync']
if args.gpu_deps:
    ports += ['qcom-hwspinlock','qcom-smem','qcom-socinfo','qcom-llcc','qcom-dcvs',
              'governor-adreno-ro','governor-gpubw','governor-adreno-tz','qcom-iommu-util','secure-buffer','iommu-logger','arm-smmu-vendor','msm-sysstats']
if args.ipc_deps or args.gpu_driver:
    ports += ['qcom-remoteproc-services','qcom-smp2p','qcom-ipcc','qcom-glink-transport','qcom-rproc-common','gunyah']
if args.gpu_driver:
    ports += ['gunyah-core','gunyah-rm','qti-fence','kgsl']
    modules += [(out/'drivers/hwtracing/coresight/coresight.ko','modules/native/coresight.ko'),
                (out/'drivers/nvmem/nvmem_qfprom.ko','modules/native/nvmem_qfprom.ko')]
if args.ice:
    ports.append('qcom-ice')
if args.ufs_host:
    ports += ['qcom-minidump','qcom-ipc-logging','ufs-host']
if args.regulators:
    ports.append('qcom-regulators')
if args.interconnect:
    ports.append('qcom-interconnect')
if args.spmi:
    ports.append('qcom-spmi')
if args.pmic_pinctrl:
    ports.append('pmic-pinctrl')
if args.adc_thermal:
    ports.append('qcom-adc-thermal')
if args.pmic_glink:
    ports.append('qcom-pmic-glink')
if args.moto_touch_deps:
    ports.append('moto-touch-deps')
if args.panel_event:
    ports.append('qcom-panel-event')
if args.power_key:
    ports.append('qcom-power-key')
if args.sdam:
    ports.append('qcom-sdam')
if args.rtc:
    ports.append('qcom-rtc')
if args.storage_phy:
    ports.append('ufs-phy-canoe')
if args.rakkashin:
    ports.append("rakkashin/adios")
    modules.append((out/"drivers/misc/ntsync.ko","modules/ntsync/ntsync.ko"))
for name in ports:
    modules += [(p,'modules/'+name+'/'+p.name) for p in (ROOT/'ports'/name).glob('*.ko')]
if args.pmic_ipc:
    modules += [(p,'modules/native-ipc/'+p.name) for p in pmic_ipc_modules()]
for p in firmware_modules():
    modules.append((p,'modules/firmware/'+p.name))
for source,name in modules:
    target=dst/name
    target.parent.mkdir(parents=True,exist_ok=True)
    shutil.copyfile(source,target)
report={
    'source_commit':subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/android-7.3'),'rev-parse','HEAD'],text=True).strip(),
    'description':('Partial port with linked KGSL; no physical boot or GPU hardware validation; not a flashable boot image' if args.gpu_driver else 'Partial port, no physical boot; KGSL not yet linked; not a flashable boot image'),
    'module_count':len(modules),'modules':sorted(name for _,name in modules),
    'image_sha256':hashlib.sha256((dst/'Image').read_bytes()).hexdigest(),
}
if args.moto_touch_deps:
    report['candidate_dtb']={'file':'candidate.dtb','sha256':hashlib.sha256((dst/'candidate.dtb').read_bytes()).hexdigest(),'whole_board_accepted':False,'scope':'stage19 candidate plus relay broker node; existing properties/order and saved GPU21 unchanged'}
(dst/'metadata.json').write_text(json.dumps(report,indent=2)+'\n')
files=sorted(p for p in dst.rglob('*') if p.is_file() and p.name!='SHA256SUMS')
(dst/'SHA256SUMS').write_text(''.join(hashlib.sha256(p.read_bytes()).hexdigest()+'  '+p.relative_to(dst).as_posix()+'\n' for p in files))
(ROOT/'reports'/(args.name+'.json')).write_text(json.dumps(report,indent=2)+'\n')
print('Staged',len(modules),'modules; Image',report['image_sha256'])
