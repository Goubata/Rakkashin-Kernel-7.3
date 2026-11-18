#!/usr/bin/env python3
"""QEMU check of the boot-kit ramdisk layout with the phone's real first-stage init.

Builds a ramdisk exactly like scripts/assemble_boot_kit.py (stock init_boot generic
ramdisk + lib/modules/<release>/ with modules.dep/.load/.alias/.softdep) but with a
few QEMU-safe GKI modules (tls, vcan whose can-dev dependency must come from
modules.dep), boots the QEMU 7.3 Image with it and checks that Android's own init:
  - runs on 7.3 (static bionic binary from the phone),
  - picks the release-specific module dir ("Release specific kernel module dir"),
  - loads the listed modules with their modules.dep dependencies,
  - then proceeds to first-stage mount (fails in QEMU: no AVR fstab/devices).
Run as root. Usage: qemu_android_init.py [--tag dryrun-first]
"""
import argparse, json, re, shlex, shutil, subprocess, sys, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts'))
from assemble_boot_kit import cpio_entries, cpio_pack, modinfo  # noqa: E402

ROOTFS = ROOT.parent / '.andronix/distros/ubuntu/rootfs'
TOOLS = ROOT.parent / 'avr-upstream-audit/tools-mkbootimg'
KOUT = 'out/native-gki'
MODS = ['tls', 'vcan', 'can_dev']
LOAD = ['tls', 'vcan']


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--timeout', type=int, default=240)
    ap.add_argument('--tree', choices=['qemu', 'prod'], default='qemu')
    a = ap.parse_args()
    global KOUT
    KOUT = {'qemu': 'out/native-gki', 'prod': 'out/prod-gki'}[a.tree]
    out = ROOT / 'tests/qemu/boot-kit-android-init'
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    release = (ROOT / KOUT / 'include/config/kernel.release').read_text().strip()
    found = {}
    for p in (ROOT / KOUT).glob('**/*.ko'):
        n, deps, al = modinfo(p)
        if n in MODS:
            found[n] = (p, deps)
    assert set(found) == set(MODS), found
    subprocess.run([sys.executable, '-I', str(TOOLS / 'unpack_bootimg.py'), '--boot_img',
                    str(ROOT / 'device-baseline/images/init_boot_a.img'), '--out', str(out / 'ib')],
                   check=True, stdout=subprocess.DEVNULL)
    subprocess.run([sys.executable, str(ROOT / 'scripts/lz4_legacy.py'), str(out / 'ib/ramdisk'), str(out / 'ib.cpio')],
                   check=True, stdout=subprocess.DEVNULL)
    entries = list(cpio_entries((out / 'ib.cpio').read_bytes()))
    entries += [('lib', 0o40755, b''), ('lib/modules', 0o40755, b''), (f'lib/modules/{release}', 0o40755, b'')]
    d = f'lib/modules/{release}/'
    for n in MODS:
        entries.append((d + found[n][0].name, 0o100644, found[n][0].read_bytes()))
    dep = '\n'.join(f'{found[n][0].name}:' + ''.join(' ' + found[x][0].name for x in found[n][1]) for n in MODS)
    entries += [(d + 'modules.dep', 0o100644, (dep + '\n').encode()),
                (d + 'modules.load', 0o100644, ('\n'.join(found[n][0].name for n in LOAD) + '\n').encode()),
                (d + 'modules.alias', 0o100644, b''), (d + 'modules.softdep', 0o100644, b''),
                (d + 'modules.blocklist', 0o100644, b'')]
    (out / 'ramdisk.cpio').write_bytes(cpio_pack(entries))
    shutil.rmtree(out / 'ib')
    (out / 'ib.cpio').unlink()
    log = ROOT / 'reports/boot-kit-android-init-qemu.log'
    cmd = ['chroot', str(ROOTFS), '/usr/bin/env', '-i', 'PATH=/usr/sbin:/usr/bin:/sbin:/bin', 'timeout', str(a.timeout),
           'qemu-system-aarch64', '-machine', 'virt,gic-version=3', '-cpu', 'max', '-smp', '2', '-m', '2048',
           '-accel', 'tcg,thread=multi', '-nodefaults', '-serial', 'stdio', '-monitor', 'none', '-display', 'none',
           '-no-reboot', '-kernel', '/work/avr-port/' + KOUT + '/arch/arm64/boot/Image',
           '-initrd', '/work/avr-port/tests/qemu/boot-kit-android-init/ramdisk.cpio',
           '-append', 'console=ttyAMA0 earlycon=pl011,0x09000000 panic=1 printk.devkmsg=on '
                      'androidboot.hardware=qcom androidboot.slot_suffix=_a']
    t = time.monotonic()
    with log.open('wb') as f:
        r = subprocess.run(['su', '-c', shlex.join(cmd)], stdout=f, stderr=subprocess.STDOUT)
    text = log.read_text(errors='replace')
    checks = {
        'init_ran': bool(re.search(r'init: init first stage started', text)),
        'release_dir': f'Release specific kernel module dir {release} found' in text,
        # module count includes modules.dep dependencies: tls + can_dev + vcan
        'loaded_with_dep': bool(re.search(r'Loaded 3 modules from /lib/modules/' + re.escape(release), text)),
        'no_insmod_failure': not re.search(r'Failed to insmod|failed to load', text),
        'no_kernel_warn': not re.search(r'WARNING:|BUG:|Oops', text),
        'reached_mount': bool(re.search(r'[Ff]irst stage mount|fstab', text)),
    }
    rep = {'release': release, 'returncode': r.returncode, 'elapsed': round(time.monotonic() - t, 1),
           'checks': checks, 'passed': all(checks.values())}
    (ROOT / 'reports/boot-kit-android-init-qemu.json').write_text(json.dumps(rep, indent=1) + '\n')
    print(json.dumps(rep))
    return 0 if rep['passed'] else 1


if __name__ == '__main__':
    sys.exit(main())
