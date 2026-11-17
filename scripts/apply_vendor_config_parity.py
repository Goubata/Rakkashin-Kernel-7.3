#!/usr/bin/env python3
"""Make a port build with the phone's vendor CONFIG set: force-include its
avr-canoe-vendor-config.h (scripts/gen_drm_vendor_config.py) and add the
Module.symvers of every port that exports a symbol the build now needs.
Usage: apply_vendor_config_parity.py <port> [<port> ...]  (idempotent)"""
import re, sys
from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
PROVIDERS = ['qcom-ipc-logging', 'qcom-pas-alor', 'qcom-rpmh', 'qcom-remoteproc-services', 'qcom-minidump',
             'qcom-debug-diag', 'qcom-perf-misc', 'qcom-dma-heaps', 'qcom-sps', 'qcom-spmi', 'qcom-display-deps',
             'qcom-rproc-common', 'qcom-smem', 'qcom-socinfo', 'qcom-iommu-util', 'secure-buffer', 'qcom-wlan-deps',
             'qcom-eom', 'qcom-pm-telemetry', 'gunyah', 'gunyah-rm', 'gunyah-core', 'qcom-llcc', 'qcom-clock-core',
             'qcom-dcvs', 'msm-sysstats', 'qcom-usb', 'qcom-leds', 'mm-drivers', 'qti-fence', 'securemsm', 'qcom-charging', 'qcom-cpucp-scmi', 'qcom-glink-transport', 'sched-walt', 'qcom-panel-event', 'qcom-regulators', 'qcom-clock-core', 'pinctrl-alor-vendor-core', 'gunyah-misc', 'geni-bus', 'qcom-thermal-misc', 'qcom-firststage-misc']
TAG = '# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)'
for port in sys.argv[1:]:
    d = ROOT / 'ports' / port
    f = d / 'Kbuild' if (d / 'Kbuild').is_file() else d / 'Makefile'  # kbuild prefers Kbuild
    full = f.read_text()
    base, _, tail = full.partition(TAG)
    s = base.rstrip('\n') + '\n'
    have = set(re.findall(r'\.\./([\w-]+)/Module\.symvers', s))
    # keep providers and extra lines added to the tagged section by hand
    kept = [m for m in re.findall(r'\.\./([\w-]+)/Module\.symvers', tail) if m not in have]
    extra = [l for l in tail.splitlines()[1:] if l.strip() and not l.startswith('KBUILD_EXTRA_SYMBOLS += $(src)/../')
             and l != 'ccflags-y += -include $(src)/avr-canoe-vendor-config.h']
    add = [p for p in dict.fromkeys(kept + PROVIDERS) if p != port and p not in have and (ROOT / 'ports' / p).is_dir()]
    s += TAG + '\nccflags-y += -include $(src)/avr-canoe-vendor-config.h\n'
    s += 'KBUILD_EXTRA_SYMBOLS += ' + ' '.join(f'$(src)/../{p}/Module.symvers' for p in add) + '\n'
    s += ''.join(l + '\n' for l in extra)
    f.write_text(s)
    print(port, f.name, 'added', len(add))
