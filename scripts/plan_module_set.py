#!/usr/bin/env python3
"""Map the phone's module sets (vendor_boot ramdisk, vendor_dlkm, system_dlkm) to 7.3 builds.

Inputs: device-baseline/dlkm-lists/* (read-only copies from the phone),
reports/module-set-decisions.json (renamed/omitted/deferred/inert), built .ko under
ports/ and out/native-gki/. Output: reports/module-set-plan.json and a summary on stdout.
Each phone module resolves to exactly one 7.3 .ko path (by modinfo name), a decision,
or 'missing'. QEMU stand-ins under tests/ are never selected.
"""
import json, re, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
L = ROOT / 'device-baseline/dlkm-lists'


def modname(p):
    try:
        out = subprocess.run(['strings', '-n', '5', str(p)], capture_output=True, text=True).stdout
    except OSError:
        return None
    m = re.search(r'^name=([\w-]+)$', out, re.M)
    return m.group(1) if m else p.stem.replace('-', '_')


def norm(n):
    return n.replace('-', '_')


def load_list(path):
    seen, out = set(), []
    for line in path.read_text().split():
        n = norm(Path(line.split(':')[0]).stem)
        if n not in seen:
            seen.add(n)
            out.append(n)
    return out


def main():
    dec = json.loads((ROOT / 'reports/module-set-decisions.json').read_text())
    renamed = {norm(k): v.get('7.3_module') for k, v in dec.get('renamed', {}).items()}
    decided = {}
    for key in ('omitted', 'deferred', 'intree'):
        for k in dec.get(key, {}):
            decided[norm(k)] = key
    for e in dec.get('not_ported_inert', []):
        decided[norm(Path(e['module']).stem)] = 'not_ported_inert'

    built = {}
    for p in sorted(list(ROOT.glob('ports/**/*.ko')) + list(ROOT.glob('out/native-gki/**/*.ko'))):
        if '/tests/' in str(p) or 'pinctrl-probe' in str(p):
            continue
        n = modname(p)
        if n and n not in built:
            built[n] = str(p.relative_to(ROOT))
    # in-tree pinctrl-alor port is built by build_native; prefer the vendor-core copies
    sets = {
        'vendor_boot': load_list(L / 'vendor_boot.modules.load.recovery') if (L / 'vendor_boot.modules.load.recovery').exists() else [],
        'vendor_boot_first_stage': load_list(L / 'vendor_boot.modules.load'),
        'vendor_dlkm': load_list(L / 'vendor_dlkm.modules.load'),
        'system_dlkm': load_list(L / 'system_dlkm.list'),
    }
    plan, summary = {}, {}
    for s, names in sets.items():
        rows, cnt = {}, {}
        for n in names:
            if decided.get(n) == 'omitted':
                r = {'decision': 'omitted'}
            elif n in built:
                r = {'ko': built[n]}
            elif n in renamed and renamed[n]:
                r = {'ko': renamed[n], 'renamed': True}
            elif n in decided:
                r = {'decision': decided[n]}
            else:
                r = {'missing': True}
            k = 'ko' if 'ko' in r else r.get('decision', 'missing')
            cnt[k] = cnt.get(k, 0) + 1
            rows[n] = r
        plan[s] = rows
        summary[s] = cnt
    (ROOT / 'reports/module-set-plan.json').write_text(json.dumps({'summary': summary, 'plan': plan}, indent=1) + '\n')
    for s in sets:
        miss = [n for n, r in plan[s].items() if r.get('missing')]
        print(s, summary[s], 'missing:', ' '.join(miss))


if __name__ == '__main__':
    sys.exit(main())
