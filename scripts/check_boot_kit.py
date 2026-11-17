#!/usr/bin/env python3
"""Static load check of an assembled boot kit (scripts/assemble_boot_kit.py).

For every module in artifacts/boot-kit-TAG/stage/lib/modules: each versioned import
must be exported by vmlinux or by another module IN THE KIT with the same CRC, the
module's depends= must name only kit modules, and vermagic must equal the kernel
release of the kit's Image. This is what finit_module() would check for the whole
set at once; it is not hardware runtime proof.
Usage: check_boot_kit.py TAG --tree prod|qemu
"""
import argparse, json, re, subprocess, sys
from pathlib import Path
sys.path.insert(0, str(Path(__file__).resolve().parent))
from audit_module_versions import versions  # noqa: E402

ROOT = Path(__file__).resolve().parents[1]
TREES = {'prod': ('out/prod-gki', 'out/prod-ports'), 'qemu': ('out/native-gki', 'ports')}


def info(p):
    out = subprocess.run(['strings', '-n', '3', str(p)], capture_output=True, text=True).stdout
    g = lambda k, pat: (re.search(rf'^{k}=({pat})$', out, re.M) or [None, ''])[1]
    return (g('name', r'[\w-]+').replace('-', '_'),
            [d.replace('-', '_') for d in g('depends', r'[\w,-]*').split(',') if d],
            g('vermagic', r'.*'))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('tag')
    ap.add_argument('--tree', choices=TREES, default='prod')
    a = ap.parse_args()
    kout, pout = TREES[a.tree]
    md = next((ROOT / 'artifacts' / f'boot-kit-{a.tag}' / 'stage/lib/modules').iterdir())
    release = (ROOT / kout / 'include/config/kernel.release').read_text().strip()
    kit = {}
    for p in sorted(md.glob('*.ko')):
        n, deps, vm = info(p)
        kit[n] = (p, deps, vm)
    # exports: vmlinux + kit members (by provider module name in Module.symvers files)
    exports = {}
    tables = [ROOT / kout / 'Module.symvers'] + sorted((ROOT / pout).glob('**/Module.symvers'))
    for t in tables:
        for line in t.read_text().splitlines():
            f = line.split('\t')
            if len(f) < 3:
                continue
            crc, sym, mod = int(f[0], 16), f[1], f[2]
            prov = 'vmlinux' if mod == 'vmlinux' else Path(mod).name.replace('-', '_')
            if prov == 'vmlinux' or prov in kit:
                exports.setdefault(sym, (crc, prov))
    problems = []
    for n, (p, deps, vm) in kit.items():
        if not vm.startswith(release + ' '):
            problems.append(f'{n}: vermagic "{vm}" != {release}')
        for d in deps:
            if d not in kit:
                problems.append(f'{n}: depends on {d} (not in kit)')
        for sym, crc in versions(p).items():
            if sym == 'module_layout':
                continue
            e = exports.get(sym)
            if e is None:
                problems.append(f'{n}: {sym} has no provider in kit/vmlinux')
            elif e[0] != crc:
                problems.append(f'{n}: {sym} CRC {crc:#x} != {e[1]} {e[0]:#x}')
    rep = {'tag': a.tag, 'tree': a.tree, 'release': release, 'modules': len(kit),
           'passed': not problems, 'problems': problems}
    (md.parents[3] / 'kit-check.json').write_text(json.dumps(rep, indent=1) + '\n')
    print(f'{len(kit)} modules, release {release}:', 'PASS' if not problems else f'{len(problems)} problems')
    for x in problems[:40]:
        print(' ', x)
    return 0 if not problems else 1


if __name__ == '__main__':
    sys.exit(main())
