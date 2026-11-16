#!/usr/bin/env python3
"""Print an insmod order (dependencies first) for target .ko files.

Resolves each module's imported symbols against the exports of every .ko found
under ports/, tests/ and out/native-gki (in-tree modules), using the real
__ksymtab_strings / __versions data in the ELF files. Symbols exported by
vmlinux (out/native-gki/Module.symvers, 'vmlinux' rows) need no module.
usage: ko_load_order.py target.ko [...]
"""
from pathlib import Path
import sys
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from audit_module_versions import sections, versions  # noqa: E402


def exports(path):
    s = sections(path.read_bytes())
    out = set()
    for name in ('__ksymtab_strings',):
        if name in s:
            out.update(x.decode() for x in s[name].split(b'\0') if x)
    return out


def main():
    vmlinux = set()
    for line in (ROOT / 'out/native-gki/Module.symvers').read_text().splitlines():
        f = line.split()
        if len(f) >= 3 and f[2] == 'vmlinux':
            vmlinux.add(f[1])
    pool = {}
    for base in ('ports', 'tests', 'out/native-gki'):
        for ko in (ROOT / base).rglob('*.ko'):
            try:
                for sym in exports(ko):
                    pool.setdefault(sym, ko)
            except Exception:
                pass
    order, seen = [], set()

    def visit(ko, stack):
        ko = ko.resolve()
        if ko in seen:
            return
        if ko in stack:
            raise SystemExit(f'cycle at {ko}')
        stack.add(ko)
        for sym in versions(ko):
            if sym in vmlinux:
                continue
            dep = pool.get(sym)
            if dep is None:
                print(f'# unresolved {sym} in {ko.name}', file=sys.stderr)
                continue
            if dep.resolve() != ko:
                visit(dep, stack)
        stack.discard(ko)
        seen.add(ko)
        order.append(ko)

    for t in sys.argv[1:]:
        visit(Path(t), set())
    for ko in order:
        print(ko.relative_to(ROOT))


if __name__ == '__main__':
    main()
