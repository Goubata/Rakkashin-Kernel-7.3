#!/usr/bin/env python3
"""Production (phone) build: lockdep-free 7.3 Image + every port rebuilt against it.

Kernel: out/prod-gki = gki_defconfig + avr-platform + rakkashin + avr-pmic-glink +
avr-production fragments (no QEMU test fixtures, no lock debugging). The core source
must be at reports/core-patch-checkpoint.json (same patched tree as the QEMU build).
Ports: sources copied to out/prod-ports/<port> (build products excluded) so the
QEMU-tested in-place .ko files are never touched; ports reference siblings through
$(src)/../<port>, which the copy preserves. Built in passes until no further port
succeeds (KBUILD_EXTRA_SYMBOLS ordering). Run as root. Usage:
  build_production.py [--kernel] [--ports] [--only port,...]
"""
import argparse, hashlib, json, os, shlex, shutil, subprocess, sys
from pathlib import Path

PORT = Path(__file__).resolve().parents[1]
ROOTFS = PORT.parent / '.andronix/distros/ubuntu/rootfs'
SOURCE = PORT.parent / 'avr-upstream-audit/android-7.3'
OUT = 'out/prod-gki'
PORTS_OUT = PORT / 'out/prod-ports'
FRAGMENTS = ['avr-platform', 'rakkashin', 'avr-pmic-glink', 'avr-production']
BUILD_EXT = ('.o', '.ko', '.mod', '.mod.c', '.mod.o', '.a', '.cmd', '.order', '.symvers', '.dwo', '.lto.o')
# ports whose module tree is a subdirectory (scripts/build_wlan.sh, build_native --rakkashin)
NESTED = ['wlan-qcacld/qcacld-3.0', 'rakkashin/adios']
JOBS = os.environ.get('BUILD_JOBS', '6')


def root_run(args, **kw):
    return subprocess.run(['su', '-c', shlex.join([str(a) for a in args])], **kw)


def chroot(cmd):
    return ['chroot', ROOTFS, '/usr/bin/env', '-i', 'PATH=/usr/sbin:/usr/bin:/sbin:/bin',
            'KBUILD_BUILD_USER=avr-builder', 'KBUILD_BUILD_HOST=avr-arm64'] + cmd


def make(extra):
    return chroot(['/usr/bin/make', '-C', '/work/avr-src', 'O=/work/avr-port/' + OUT,
                   'ARCH=arm64', 'LLVM=1'] + extra)


def check_source():
    cp = json.loads((PORT / 'reports/core-patch-checkpoint.json').read_text())
    head = subprocess.check_output(['git', '-C', str(SOURCE), 'rev-parse', 'HEAD'], text=True).strip()
    dirty = subprocess.check_output(['git', '-C', str(SOURCE), 'status', '--porcelain'], text=True).strip()
    if head != cp['source_commit'] or dirty:
        raise SystemExit(f'core source not at checkpoint ({head[:12]} vs {cp["source_commit"][:12]}, dirty={bool(dirty)})')


def kernel():
    check_source()
    (PORT / OUT).mkdir(parents=True, exist_ok=True)
    root_run(make(['gki_defconfig']), check=True)
    for f in FRAGMENTS:
        root_run(chroot(['/bin/bash', '/work/avr-src/scripts/kconfig/merge_config.sh', '-m', '-O',
                         '/work/avr-port/' + OUT, f'/work/avr-port/{OUT}/.config',
                         f'/work/avr-port/configs/{f}.fragment']), check=True)
        root_run(make(['olddefconfig']), check=True)
    cfg = (PORT / OUT / '.config').read_text()
    for bad in ('CONFIG_PROVE_LOCKING=y', 'CONFIG_LOCKDEP=y', 'CONFIG_KUNIT=m', 'CONFIG_KUNIT=y'):
        if bad + '\n' in cfg:
            raise SystemExit(f'production config still has {bad}')
    root_run(make(['-j' + JOBS, 'Image', 'modules']), check=True)


def copy_ports(only):
    for src in sorted((PORT / 'ports').iterdir()):
        if not src.is_dir() or (only and src.name not in only):
            continue
        dst = PORTS_OUT / src.name
        for p in src.rglob('*'):
            rel = p.relative_to(src)
            q = dst / rel
            if p.is_symlink():
                if not q.is_symlink():
                    q.parent.mkdir(parents=True, exist_ok=True)
                    if q.exists():
                        q.unlink()
                    q.symlink_to(os.readlink(p))
                continue
            if p.is_dir() or p.name.endswith(BUILD_EXT) or p.name.startswith('.') and p.name.endswith('.cmd'):
                continue
            if q.exists() and q.stat().st_mtime >= p.stat().st_mtime and q.stat().st_size == p.stat().st_size:
                continue
            q.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(p, q)


def buildable(d):
    return (d / 'Makefile').is_file() or (d / 'Kbuild').is_file()


def ports(only):
    copy_ports(only)
    todo = [d for d in sorted(PORTS_OUT.iterdir()) if d.is_dir() and buildable(d) and (not only or d.name in only)]
    todo += [PORTS_OUT / n for n in NESTED if (not only or n.split('/')[0] in only) and buildable(PORTS_OUT / n)]
    # pinctrl-alor is the in-tree style copy built by build_native; production uses the vendor core
    todo = [d for d in todo if d.name != 'pinctrl-alor']
    for d in todo:
        for f in d.glob('Module.symvers'):
            f.unlink()
    passes = 0
    while todo:
        passes += 1
        failed = []
        for d in todo:
            rel = d.relative_to(PORTS_OUT).as_posix()
            r = root_run(make([f'M=/work/avr-port/out/prod-ports/{rel}', '-j' + JOBS, 'modules']),
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            (failed.append(d) if r.returncode else print(f'pass{passes} ok {rel}', flush=True))
        if len(failed) == len(todo):
            print('FAILED:', ' '.join(d.relative_to(PORTS_OUT).as_posix() for d in failed))
            return 1
        todo = failed
    return 0


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--kernel', action='store_true')
    ap.add_argument('--ports', action='store_true')
    ap.add_argument('--only', default='')
    a = ap.parse_args()
    only = set(filter(None, a.only.split(',')))
    if a.kernel:
        kernel()
    if a.ports:
        return ports(only)
    return 0


if __name__ == '__main__':
    sys.exit(main())
