#!/usr/bin/env python3
"""Recompute the module list of a make_qemu_initramfs.py test block from its
port modules with scripts/ko_load_order.py (picks up dependencies added later,
e.g. by vendor CONFIG parity), keeping QEMU stand-ins and test modules:
  gunyah-core -> tests/dma-heaps/avr-gunyah-stub.ko, real qcom-scm -> fake-scm,
  ipclite+synx-driver -> tests/msm-drm/avr-synx-stub.ko (when they were stubbed),
  tests/*.ko keep their relative position at the end.
Usage: refresh_test_modules.py <flag> [<flag> ...]   (e.g. qcom_pm_tests)"""
import re, subprocess, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
p = ROOT / 'scripts/make_qemu_initramfs.py'
s = p.read_text()
for flag in sys.argv[1:]:
    i = s.index(f'    if args.{flag}:')
    a = s.index('sources=[', i)
    depth = 0
    for j in range(a + len('sources='), len(s)):
        if s[j] == '[': depth += 1
        elif s[j] == ']':
            depth -= 1
            if depth == 0: break
    old = s[a:j + 1]
    paths = re.findall(r"ROOT/'([^']+)'", old)
    stubs = {x for x in paths if x.startswith('tests/') and ('stub' in x or x.endswith('fake-scm/qcom-scm.ko'))}
    tests = [x for x in paths if x.startswith('tests/') and x not in stubs]
    kunit = [x for x in paths if x.endswith('lib/kunit/kunit.ko')]
    ports = [x for x in paths if not x.startswith('tests/') and x not in kunit]
    order = subprocess.check_output([sys.executable, '-I', str(ROOT / 'scripts/ko_load_order.py')] + [str(ROOT / x) for x in ports], text=True, cwd=ROOT).split()
    order = [str(Path(o).resolve().relative_to(ROOT)) if Path(o).is_absolute() else o for o in order]
    out = []
    for x in order:
        # QEMU virt is never a Gunyah guest: always use the CRC-equal stub
        if x.startswith('ports/gunyah-core/'):
            x = 'tests/dma-heaps/avr-gunyah-stub.ko'
        if x.endswith('gh_rm_heap_manager.ko'):
            continue
        if 'tests/qcom-tee/fake-scm/qcom-scm.ko' in stubs and x.endswith(('firmware/qcom/qcom-scm.ko', 'fake-scm/qcom-scm.ko')):
            x = 'tests/qcom-tee/fake-scm/qcom-scm.ko'
        # qcom-reboot: minidump stand-in (msm_minidump_enabled() == true, no SMEM table)
        if 'tests/qcom-reboot/avr-minidump-stub.ko' in stubs and x == 'ports/qcom-minidump/minidump.ko':
            x = 'tests/qcom-reboot/avr-minidump-stub.ko'
        if 'tests/msm-drm/avr-synx-stub.ko' in stubs and x.endswith(('ipclite.ko', 'synx-driver.ko')):
            x = 'tests/msm-drm/avr-synx-stub.ko'
        if x not in out:
            out.append(x)
    new = 'sources=[' + ','.join(f"ROOT/'{x}'" for x in kunit + out + tests) + ']'
    s = s[:a] + new + s[j + 1:]
    print(flag, len(paths), '->', len(kunit + out + tests))
p.write_text(s)
