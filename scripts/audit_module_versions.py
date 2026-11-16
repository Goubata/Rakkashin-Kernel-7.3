#!/usr/bin/env python3
"""Check real module import CRCs against the current core and port exports."""
import argparse
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]

def sections(data):
    if data[:6]!=b'\x7fELF\x02\x01' or struct.unpack_from('<H',data,18)[0]!=183:raise ValueError('expected AArch64 ELF64 LE')
    offset=struct.unpack_from('<Q',data,40)[0]
    size,count,names_index=struct.unpack_from('<HHH',data,58)
    headers=[struct.unpack_from('<IIQQQQIIQQ',data,offset+i*size) for i in range(count)]
    n=headers[names_index];names=data[n[4]:n[4]+n[5]]
    return {names[h[0]:].split(b'\0',1)[0].decode():data[h[4]:h[4]+h[5]] for h in headers}

def versions(path):
    s=sections(path.read_bytes())
    result={}
    if '__versions' in s:
        data=s['__versions']
        if len(data)%64:raise ValueError(f'{path}: unexpected modversion size')
        result.update({data[i+8:i+64].split(b'\0',1)[0].decode():struct.unpack_from('<Q',data,i)[0] for i in range(0,len(data),64)})
    if '__version_ext_names' in s:
        names=s['__version_ext_names'].rstrip(b'\0').split(b'\0')
        crcs=s['__version_ext_crcs']
        if len(crcs)!=len(names)*4:raise ValueError(f'{path}: invalid extended versions')
        for name,crc in zip((n.decode() for n in names),struct.unpack('<'+'I'*len(names),crcs)):
            if name in result and result[name]!=crc:raise ValueError(f'{path}: conflicting CRCs for {name}')
            result[name]=crc
    if not result:raise ValueError(f'{path}: no version information')
    return result

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module',type=Path,nargs='*')
    parser.add_argument('--stage',type=Path,help='Recursively inspect every .ko under a staging directory')
    args=parser.parse_args()
    if args.stage and args.module:parser.error('Choose --stage or explicit module files')
    tables=[ROOT/'out/native-gki/Module.symvers']
    tables += [ROOT/'ports'/n/'Module.symvers' for n in ['qcom-clock-core','qcom-rpmh','qcom-clock-controllers','qcom-irq','pinctrl-alor-vendor-core','avr-oplus-sync','qcom-smem','qcom-socinfo','qcom-hwspinlock','qcom-llcc','qcom-dcvs','governor-adreno-ro','governor-gpubw','governor-adreno-tz','qcom-iommu-util','secure-buffer','iommu-logger','arm-smmu-vendor','msm-sysstats','rakkashin/adios','qcom-rproc-common','gunyah','gunyah-core','gunyah-rm','qti-fence','kgsl','ufs-phy-canoe','qcom-ice','qcom-minidump','qcom-ipc-logging','ufs-host','qcom-regulators','qcom-interconnect','qcom-spmi','pmic-pinctrl','qcom-adc-thermal','qcom-pmic-glink','qcom-glink-transport','qcom-ipcc','qcom-smp2p','qcom-remoteproc-services','qcom-pas-alor','qcom-power-key','qcom-panel-event','moto-touch-deps','moto-info','goodix-touch','usb-repeater','geni-bus','qcom-tsens','qcom-rng','qcom-watchdog','qcom-bcl-soc','qcom-bcl-pmic','sched-walt','qcom-dma-heaps','qcom-tee','securemsm','qcom-reboot']]
    exports={}
    for table in tables:
        if not table.is_file():continue
        for line in table.read_text().splitlines():
            fields=line.split()
            if fields:exports[fields[1]]=int(fields[0],16)
    paths=args.module or sorted((args.stage or ROOT/'artifacts/android73-boot-core-stage2/modules').rglob('*.ko'))
    if not paths:raise SystemExit("No module files selected")
    results=[]
    for path in paths:
        imports=versions(path)
        mismatches={name:{'module':hex(crc),'provider':hex(exports[name]) if name in exports else None} for name,crc in imports.items() if exports.get(name)!=crc}
        results.append({'module':str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path),'imports_checked':len(imports),'mismatches':mismatches})
    report={'scope':'import CRC consistency only; not hardware runtime proof','passed':all(not r['mismatches'] for r in results),'modules':results}
    print(json.dumps(report,indent=2))
    raise SystemExit(not report['passed'])

if __name__=='__main__':main()
