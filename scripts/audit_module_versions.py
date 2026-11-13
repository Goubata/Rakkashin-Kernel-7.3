#!/usr/bin/env python3
"""Check real module import CRCs against the current core and port exports."""
import argparse
import json
from pathlib import Path
import struct

ROOT=Path(__file__).resolve().parents[1]

def sections(data):
    if data[:6]!=b'\x7fELF\x02\x01':raise ValueError('expected ELF64 LE')
    offset=struct.unpack_from('<Q',data,40)[0]
    size,count,names_index=struct.unpack_from('<HHH',data,58)
    headers=[struct.unpack_from('<IIQQQQIIQQ',data,offset+i*size) for i in range(count)]
    n=headers[names_index];names=data[n[4]:n[4]+n[5]]
    return {names[h[0]:].split(b'\0',1)[0].decode():data[h[4]:h[4]+h[5]] for h in headers}

def versions(path):
    s=sections(path.read_bytes())
    if '__version_ext_names' in s:
        names=s['__version_ext_names'].rstrip(b'\0').split(b'\0')
        crcs=s['__version_ext_crcs']
        if len(crcs)!=len(names)*4:raise ValueError(f'{path}: invalid extended versions')
        return dict(zip((n.decode() for n in names),struct.unpack('<'+'I'*len(names),crcs)))
    if '__versions' in s:
        data=s['__versions']
        if len(data)%64:raise ValueError(f'{path}: unexpected modversion size')
        return {data[i+8:i+64].split(b'\0',1)[0].decode():struct.unpack_from('<Q',data,i)[0] for i in range(0,len(data),64)}
    raise ValueError(f'{path}: no version information')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('module',type=Path,nargs='*')
    args=parser.parse_args()
    tables=[ROOT/'out/native-gki/Module.symvers']
    tables += [ROOT/'ports'/n/'Module.symvers' for n in ['qcom-clock-core','qcom-rpmh','qcom-clock-controllers','qcom-irq','pinctrl-alor-vendor-core','avr-oplus-sync']]
    exports={}
    for table in tables:
        for line in table.read_text().splitlines():
            fields=line.split()
            if fields:exports[fields[1]]=int(fields[0],16)
    paths=args.module or sorted((ROOT/'artifacts/android73-boot-core-stage2/modules').rglob('*.ko'))
    results=[]
    for path in paths:
        imports=versions(path)
        mismatches={name:{'module':hex(crc),'provider':hex(exports[name]) if name in exports else None} for name,crc in imports.items() if exports.get(name)!=crc}
        results.append({'module':str(path.relative_to(ROOT) if path.is_relative_to(ROOT) else path),'imports_checked':len(imports),'mismatches':mismatches})
    report={'scope':'import CRC consistency only; not hardware runtime proof','passed':all(not r['mismatches'] for r in results),'modules':results}
    print(json.dumps(report,indent=2))
    raise SystemExit(not report['passed'])

if __name__=='__main__':main()
