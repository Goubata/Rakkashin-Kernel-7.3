#!/usr/bin/env python3
"""Assemble phone boot artifacts for the 7.3 kernel (image creation only; never flashes).

Module selection comes from reports/module-set-plan.json (scripts/plan_module_set.py),
remapped to the chosen build tree, plus the dependency closure of every selected
module (modinfo depends=). Phone blocklists (vendor_dlkm / system_dlkm) are honoured.

--kind ramtest  boot-ramtest.img for `fastboot boot` (nothing written to the phone):
                header v4, 7.3 Image, ramdisk = stock init_boot generic ramdisk +
                lib/modules/<kernel release>/. AOSP first_stage_init loads only from
                a release-specific module dir when one exists, so the 6.12 modules
                in the flashed vendor_boot ramdisk are ignored; the bootloader still
                provides the flashed vendor_boot DTB, cmdline and bootconfig.
                First-stage loading is strict on user builds (any failure ->
                LOG(FATAL) -> reboot to bootloader), so modules.load lists only
                modules that are loaded on the running phone (loaded-modules.txt)
                plus the 7.3 providers they depend on; other files stay unlisted.
                vendor_dlkm/system_dlkm on the phone stay 6.12 and are rejected by
                the 7.3 kernel (vermagic) when vendor init tries them.
--set first     only the phone's first-stage (vendor_boot modules.load) set
--set full      first stage + system_dlkm + vendor_dlkm (all loaded in first stage)

Usage: assemble_boot_kit.py --tree prod|qemu --kind ramtest --set first|full --tag NAME
Output: artifacts/boot-kit-NAME/ (+ SHA256SUMS, manifest.json).
"""
import argparse, hashlib, json, re, shlex, shutil, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
L = ROOT / 'device-baseline/dlkm-lists'
IMG = ROOT / 'device-baseline/images'
TOOLS = ROOT.parent / 'avr-upstream-audit/tools-mkbootimg'
ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'
TREES = {'prod': ('out/prod-gki', 'out/prod-ports'), 'qemu': ('out/native-gki', 'ports')}


def modinfo(p):
    out = subprocess.run(['strings', '-n', '3', str(p)], capture_output=True, text=True).stdout
    name = re.search(r'^name=([\w-]+)$', out, re.M)
    deps = re.search(r'^depends=([\w,-]*)$', out, re.M)
    aliases = re.findall(r'^alias=(\S+)$', out, re.M)
    return ((name.group(1) if name else p.stem).replace('-', '_'),
            [d.replace('-', '_') for d in (deps.group(1).split(',') if deps else []) if d], aliases)


def blocklist(path):
    if not path.exists():
        return set()
    return {l.split()[1].replace('-', '_') for l in path.read_text().splitlines() if l.startswith('blocklist ')}


def cpio_entries(raw):
    i = 0
    while i < len(raw):
        h = raw[i:i + 110]
        f = [int(h[6 + 8 * k:14 + 8 * k], 16) for k in range(13)]
        fs, ns = f[6], f[11]
        name = raw[i + 110:i + 110 + ns - 1].decode()
        i = (i + 110 + ns + 3) & ~3
        data = raw[i:i + fs]
        i = (i + fs + 3) & ~3
        if name == 'TRAILER!!!':
            return
        yield name, f[1], data


def cpio_pack(entries):
    out = bytearray()
    ino = 300000
    for name, mode, data in entries + [('TRAILER!!!', 0, b'')]:
        ino += 1
        nb = name.encode() + b'\0'
        hdr = '070701' + ''.join('%08X' % v for v in (ino, mode, 0, 0, 1, 0, len(data), 0, 0, 0, 0, len(nb), 0))
        out += hdr.encode() + nb
        out += b'\0' * ((-len(out)) % 4)
        out += data
        out += b'\0' * ((-len(out)) % 4)
    return bytes(out)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--tree', choices=TREES, default='prod')
    ap.add_argument('--kind', choices=['ramtest'], default='ramtest')
    ap.add_argument('--set', choices=['first', 'full'], default='full')
    ap.add_argument('--tag', required=True)
    ap.add_argument('--defer', default='', help='comma list: keep files but do not auto-load these '
                    'modules nor anything depending on them (bring-up: load by hand over adb)')
    a = ap.parse_args()
    kout, pout = TREES[a.tree]
    image = ROOT / kout / 'arch/arm64/boot/Image'
    release = (ROOT / kout / 'include/config/kernel.release').read_text().strip()
    loaded = {l.split()[0] for l in (ROOT / 'device-baseline/loaded-modules.txt').read_text().splitlines() if l.strip()}
    if not image.exists():
        raise SystemExit(f'missing {image}')

    # every candidate .ko in the chosen tree, by module name
    cands = {}
    # vendor ports win over same-named 7.3 in-tree modules (e.g. vendor qcom_ice for UFS FBE)
    for p in sorted((ROOT / pout).glob('**/*.ko')) + sorted((ROOT / kout).glob('**/*.ko')):
        if '/tests/' in str(p) or 'pinctrl-probe' in str(p) or '/pinctrl-alor/' in str(p):
            continue
        n, deps, al = modinfo(p)
        cands.setdefault(n, (p, deps, al))

    def remap(rel):
        for src, dst in (('ports/', pout + '/'), ('out/native-gki/', kout + '/')):
            if rel.startswith(src):
                return dst + rel[len(src):]
        return rel

    plan = json.loads((ROOT / 'reports/module-set-plan.json').read_text())['plan']
    block = blocklist(L / 'vendor_dlkm.modules.blocklist') | blocklist(L / 'system_dlkm.modules.blocklist')
    order, missing, listed = [], [], set()

    def want(rows, honour_block):
        for n, r in rows.items():
            if 'ko' not in r or (honour_block and n in block):
                continue
            ko = r['ko']
            if ko.startswith('built-in'):
                continue
            if (ROOT / remap(ko)).exists():
                mn = modinfo(ROOT / remap(ko))[0]
            else:   # renamed entries may give a bare file name (e.g. scmi-clk.ko)
                mn = next((k for k, v in cands.items() if v[0].name == Path(ko).name), None)
            if mn not in cands:
                missing.append(r['ko'])
                continue
            if mn not in order:
                order.append(mn)
            if n in loaded or mn in loaded:
                listed.add(mn)

    want(plan['vendor_boot_first_stage'], False)
    if a.set == 'full':
        want(plan['system_dlkm'], True)
        want(plan['vendor_dlkm'], True)
    # dependency closure (7.3 may split/rename providers)
    i = 0
    while i < len(order):
        for d in cands[order[i]][1]:
            if d not in cands:
                missing.append(f'{order[i]} -> {d}')
            elif d not in order:
                order.append(d)
            if order[i] in listed:
                listed.add(d)       # providers of listed modules load too
        i += 1
    # propagate to transitive providers
    changed = True
    while changed:
        changed = False
        for n in list(listed):
            for d in cands[n][1]:
                if d not in listed:
                    listed.add(d)
                    changed = True
    if missing:
        print('UNRESOLVED:', *sorted(set(missing)), sep='\n  ')
        return 1

    outd = ROOT / 'artifacts' / f'boot-kit-{a.tag}'
    if outd.exists():
        shutil.rmtree(outd)
    md = outd / 'stage/lib/modules' / release
    md.mkdir(parents=True)
    files = {}
    for n in order:
        src = cands[n][0]
        dst = md / src.name
        if dst.exists():
            raise SystemExit(f'file name clash {src.name}')
        shutil.copy2(src, dst)
        subprocess.run(['strip', '--strip-debug', str(dst)], check=True)
        files[n] = src.name

    # modules.dep: transitive deps, deepest last (libmodprobe loads in reverse)
    def closure(n):
        """deps of n ordered so that every module precedes its own deps (reverse post-order)."""
        post, seen = [], set()

        def visit(m):
            seen.add(m)
            for d in cands[m][1]:
                if d not in seen:
                    visit(d)
            post.append(m)
        visit(n)
        return list(reversed(post[:-1]))
    dep_lines = []
    for n in order:
        deps = closure(n)
        for i, d in enumerate(deps):   # libmodprobe loads the list back to front
            if any(x in deps[:i] for x in cands[d][1]):
                raise SystemExit(f'modules.dep order broken for {n}: {d}')
        dep_lines.append(f'{files[n]}:' + ''.join(f' {files[d]}' for d in deps))   # relative to the dir
    alias_lines = ['# Aliases extracted from modules themselves.'] + \
        [f'alias {al} {n}' for n in order for al in cands[n][2]]
    (md / 'modules.dep').write_text('\n'.join(dep_lines) + '\n')
    (md / 'modules.alias').write_text('\n'.join(alias_lines) + '\n')
    deferred = {d.replace('-', '_') for d in a.defer.split(',') if d}
    if deferred:
        changed = True
        while changed:
            changed = False
            for n in list(listed):
                if n not in deferred and any(d in deferred for d in cands[n][1]):
                    deferred.add(n)
                    changed = True
        listed -= deferred
        print('deferred (not auto-loaded):', ' '.join(sorted(deferred)))
    # load order: providers before users (reverse post-order of the whole listed set)
    seq, seen = [], set()

    def visit(m):
        seen.add(m)
        for d in cands[m][1]:
            if d not in seen:
                visit(d)
        seq.append(m)
    for n in order:
        if n in listed and n not in seen:
            visit(n)
    (md / 'modules.load').write_text('\n'.join(files[n] for n in seq if n in listed) + '\n')
    soft = [l for f in ('vendor_boot.modules.softdep', 'vendor_dlkm.modules.softdep') if (L / f).exists()
            for l in (L / f).read_text().splitlines() if l.startswith('softdep ')]
    soft = [l for l in dict.fromkeys(soft) if all(t.replace('-', '_') in cands for t in l.split()[1:] if t != 'pre:' and t != 'post:')]
    (md / 'modules.softdep').write_text('\n'.join(soft) + '\n')
    (md / 'modules.blocklist').write_text('')

    # ramdisk = stock generic ramdisk + our lib/modules
    ib = outd / 'init_boot-unpacked'
    subprocess.run([sys.executable, '-I', str(TOOLS / 'unpack_bootimg.py'), '--boot_img',
                    str(IMG / 'init_boot_a.img'), '--out', str(ib)], check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, str(ROOT / 'scripts/lz4_legacy.py'), str(ib / 'ramdisk'), str(ib / 'ramdisk.cpio')],
                   check=True, stdout=subprocess.DEVNULL)
    raw = (ib / 'ramdisk.cpio').read_bytes()
    entries = list(cpio_entries(raw))
    names = {e[0] for e in entries}
    for d in ('lib', 'lib/modules', f'lib/modules/{release}'):
        if d not in names:
            entries.append((d, 0o40755, b''))
    for p in sorted(md.iterdir()):
        entries.append((f'lib/modules/{release}/{p.name}', 0o100644, p.read_bytes()))
    # newc cpio, LZ4 legacy frames like the stock ramdisks (lz4 from the build chroot; run as root)
    cpio = outd / 'ramdisk.cpio'
    cpio.write_bytes(cpio_pack(entries))
    rd = outd / 'ramdisk.cpio.lz4'
    rel = '/work/avr-port/' + str(outd.relative_to(ROOT))
    subprocess.run(['su', '-c', shlex.join(['chroot', str(ROOTFS), '/usr/bin/lz4', '-l', '-12', '--favor-decSpeed',
                                            '-f', '-q', rel + '/ramdisk.cpio', rel + '/ramdisk.cpio.lz4'])], check=True)
    cpio.unlink()
    boot = outd / 'boot-ramtest.img'
    subprocess.run([sys.executable, '-E', '-s', str(TOOLS / 'mkbootimg.py'), '--header_version', '4',
                    '--kernel', str(image), '--ramdisk', str(rd), '--os_version', '16.0.0',
                    '--os_patch_level', '2026-06', '--output', str(boot)], check=True)
    shutil.rmtree(ib)
    manifest = {'kind': a.kind, 'set': a.set, 'deferred': sorted(deferred), 'release': release, 'listed': len([n for n in seq if n in listed]),
                'unlisted': sorted(files[n] for n in order if n not in listed), 'tree': a.tree, 'image': str(image.relative_to(ROOT)),
                'image_sha256': hashlib.sha256(image.read_bytes()).hexdigest(),
                'modules': len(order), 'boot_img_bytes': boot.stat().st_size,
                'load_order': [files[n] for n in seq if n in listed]}
    (outd / 'manifest.json').write_text(json.dumps(manifest, indent=1) + '\n')
    sums = [f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}' for p in (boot, rd)]
    (outd / 'SHA256SUMS').write_text('\n'.join(sums) + '\n')
    print(f'{boot}: {len(order)} modules ({manifest["listed"]} in modules.load), release {release}, {boot.stat().st_size} bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main())
