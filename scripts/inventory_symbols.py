#!/usr/bin/env python3
"""Read ELF module imports without loading or executing any module."""
from pathlib import Path
import json
import struct
import tarfile

PORT = Path(__file__).resolve().parents[1]
BASE = PORT/'device-baseline'

def inspect(data):
    if data[:6] != b'\x7fELF\x02\x01':
        raise ValueError('Expected little-endian ELF64 module')
    if struct.unpack_from('<H', data, 18)[0] != 183:
        raise ValueError('Expected AArch64 module')
    offset = struct.unpack_from('<Q', data, 40)[0]
    esize, count, stridx = struct.unpack_from('<HHH', data, 58)
    sections = [struct.unpack_from('<IIQQQQIIQQ', data, offset+i*esize) for i in range(count)]
    names_hdr = sections[stridx]
    names = data[names_hdr[4]:names_hdr[4]+names_hdr[5]]
    imports = set()
    exports = set()
    metadata = []
    for s in sections:
        name = names[s[0]:].split(b'\0', 1)[0]
        if name == b'.modinfo':
            metadata = data[s[4]:s[4]+s[5]].rstrip(b'\0').decode().split('\0')
        if s[1] != 2:
            continue
        strings_hdr = sections[s[6]]
        strings = data[strings_hdr[4]:strings_hdr[4]+strings_hdr[5]]
        for pos in range(s[4], s[4]+s[5], s[9]):
            n, info, other, idx, value, size = struct.unpack_from('<IBBHQQ', data, pos)
            name = strings[n:].split(b'\0', 1)[0].decode() if n else ''
            if idx == 0 and n and info >> 4 in (1, 2):
                imports.add(name)
            if idx != 0 and name.startswith('__ksymtab_'):
                exports.add(name[len('__ksymtab_'):])
            if idx != 0 and name.startswith('__export_symbol_'):
                exports.add(name[len('__export_symbol_'):])
    return {'imports': sorted(imports), 'exports': sorted(exports),
            'vermagic': next((m[9:] for m in metadata if m.startswith('vermagic=')), None)}

def main():
    modules = {}
    for kind in ('vendor', 'system'):
        with tarfile.open(BASE/f'{kind}-modules.tar') as archive:
            for item in archive:
                if item.isfile() and item.name.endswith('.ko'):
                    modules[kind+'/'+item.name] = inspect(archive.extractfile(item).read())
    for path in (BASE/'early-modules').rglob('*.ko'):
        modules[str(path.relative_to(BASE))] = inspect(path.read_bytes())
    symbols = sorted({s for m in modules.values() for s in m['imports']})
    (PORT/'reports/module-imports.json').write_text(json.dumps({'modules': modules, 'required_symbols': symbols}, indent=2)+'\n')
    symvers = PORT/'out/native-gki/Module.symvers'
    if symvers.is_file():
        baseline = {l.split()[1] for l in symvers.read_text().splitlines() if l.strip()}
        archived = {s for m in modules.values() for s in m['exports']}
        missing = sorted(set(symbols) - baseline - archived)
        report = {'scope': 'symbol-name inventory only; source/API porting can change imports',
                  'baseline_export_count': len(baseline), 'archived_module_export_count': len(archived),
                  'unmatched_import_names': missing}
        (PORT/'reports/baseline-symbol-gaps.json').write_text(json.dumps(report, indent=2)+'\n')
        print(f'{len(missing)} imported names absent from baseline and archived module exports')
    print(f'{len(modules)} module files; {len(symbols)} distinct imported symbols')

if __name__ == '__main__':
    main()
