#!/usr/bin/env python3
"""Dual vendor_boot: the stock (6.12) vendor ramdisk plus lib/modules/<7.3 release>/.

The phone's bootloader takes the generic ramdisk from init_boot even for
`fastboot boot` (stage126 phone test: 7.3 booted but first-stage init loaded the
flat 6.12 /lib/modules), so 7.3 modules must come from vendor_boot. AOSP first-stage
init loads only from /lib/modules/<uname -r>/ when it exists and skips directories
of other kernel versions, so:
  - 6.12 boots: release dir 7.3.0-4k is skipped -> flat 6.12 modules as before;
  - 7.3 boots (fastboot boot boot-73.img): only lib/modules/7.3.0-4k/ is used.
Everything else (DTB, vendor cmdline, bootconfig, header fields, page size) is the
stock image's; repacking the stock inputs unchanged is bit-identical (checked).
Outputs in artifacts/boot-kit-TAG/: vendor_boot-dual.img (AVB hash footer, algorithm
NONE like stock, partition size 100663296) and boot-73.img (kernel only, header v4).
Usage: assemble_vendor_boot_dual.py TAG     (TAG of an assembled boot kit). Run as root.
"""
import hashlib, re, shlex, subprocess, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
from assemble_boot_kit import cpio_entries, cpio_pack, ROOTFS, TOOLS  # noqa: E402

IMG = ROOT / 'device-baseline/images'
AVB = ROOT.parent / 'avr-upstream-audit/oxygenos-port-reference/project/tools/avbtool.py'
PART = 100663296
FPR = 'motorola/avr_g/avr:16/WWW36V.65-25-3-ST9.3/fc488:user/release-keys'


def run(*a, **k):
    return subprocess.run(list(a), check=True, **k)


def main():
    tag = sys.argv[1]
    kit = ROOT / 'artifacts' / f'boot-kit-{tag}'
    md = next((kit / 'stage/lib/modules').iterdir())
    release = md.name
    tree = 'out/prod-gki' if 'prod' in tag else 'out/native-gki'
    image = ROOT / tree / 'arch/arm64/boot/Image'
    work = kit / 'vb-work'
    run(sys.executable, '-I', str(TOOLS / 'unpack_bootimg.py'), '--boot_img', str(IMG / 'vendor_boot_a.img'),
        '--out', str(work), '--format', 'mkbootimg', stdout=open(kit / 'vb-args.txt', 'w'))
    args = (kit / 'vb-args.txt').read_text()
    cmdline = re.search(r"--vendor_cmdline '([^']*)'", args).group(1)
    run(sys.executable, str(ROOT / 'scripts/lz4_legacy.py'), str(work / 'vendor_ramdisk00'), str(work / 'vr.cpio'),
        stdout=subprocess.DEVNULL)
    entries = list(cpio_entries((work / 'vr.cpio').read_bytes()))
    names = {e[0] for e in entries}
    if any(n.startswith(f'lib/modules/{release}') for n in names):
        raise SystemExit('stock ramdisk already has a release dir?')
    entries.append((f'lib/modules/{release}', 0o40755, b''))
    for p in sorted(md.iterdir()):
        entries.append((f'lib/modules/{release}/{p.name}', 0o100644, p.read_bytes()))
    (work / 'dual.cpio').write_bytes(cpio_pack(entries))
    rel = '/work/avr-port/' + str(work.relative_to(ROOT))
    run('su', '-c', shlex.join(['chroot', str(ROOTFS), '/usr/bin/lz4', '-l', '-12', '--favor-decSpeed', '-f', '-q',
                                rel + '/dual.cpio', rel + '/dual.cpio.lz4']))
    vb = kit / 'vendor_boot-dual.img'
    run(sys.executable, '-E', '-s', str(TOOLS / 'mkbootimg.py'), '--header_version', '4', '--pagesize', '0x1000',
        '--base', '0', '--kernel_offset', '0x8000', '--ramdisk_offset', '0x01000000', '--tags_offset', '0x100',
        '--dtb_offset', '0x1f00000', '--vendor_cmdline', cmdline, '--board', '', '--dtb', str(work / 'dtb'),
        '--vendor_bootconfig', str(work / 'bootconfig'), '--ramdisk_type', '1', '--ramdisk_name', '',
        '--vendor_ramdisk_fragment', str(work / 'dual.cpio.lz4'), '--vendor_boot', str(vb))
    run(sys.executable, '-I', str(AVB), 'add_hash_footer', '--image', str(vb), '--partition_name', 'vendor_boot',
        '--partition_size', str(PART), '--algorithm', 'NONE',
        '--prop', f'com.android.build.vendor_boot.fingerprint:{FPR}')
    boot = kit / 'boot-73.img'
    run(sys.executable, '-E', '-s', str(TOOLS / 'mkbootimg.py'), '--header_version', '4', '--kernel', str(image),
        '--os_version', '16.0.0', '--os_patch_level', '2026-06', '--output', str(boot))
    run(sys.executable, '-I', str(AVB), 'add_hash_footer', '--image', str(boot), '--partition_name', 'boot',
        '--partition_size', str(PART), '--algorithm', 'NONE',
        '--prop', 'com.android.build.boot.os_version:16', '--prop', f'com.android.build.boot.fingerprint:{FPR}',
        '--prop', 'com.android.build.boot.security_patch:2026-06-01')
    for f in ('vr.cpio', 'dual.cpio'):
        (work / f).unlink()
    sums = [f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}' for p in (vb, boot)]
    (kit / 'SHA256SUMS-dual').write_text('\n'.join(sums) + '\n')
    print('\n'.join(sums))
    print(f'{vb}: {vb.stat().st_size} bytes (ramdisk {(work / "dual.cpio.lz4").stat().st_size}), release dir {release}')
    return 0


if __name__ == '__main__':
    sys.exit(main())
