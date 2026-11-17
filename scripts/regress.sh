#!/bin/sh
# Full QEMU regression on the current Image: usage regress.sh TAG  (run as root)
# Writes reports/regress-TAG-*.log/json and reports/regress-TAG-summary.txt.
set -u
T=$1; cd "$(dirname "$0")/.."
one() { # name extra-append
  python3 scripts/run_qemu.py --test "$1" --cpus 4 --timeout 300 ${2:+--append "$2"} --log reports/regress-$T-$1.log >/dev/null 2>&1
  python3 scripts/check_qemu_log.py --test "$1" reports/regress-$T-$1.log 2>/dev/null | python3 -c "import json,sys
try:
 d=json.load(sys.stdin);print('$1',d['passed'],d.get('totals',d.get('checks','')),d['failures'][:2])
except Exception as e: print('$1',False,'check-error',e)"
}
for t in smoke clock oplus rakkashin ntsync fence walt-hooks ddr-cooling devfreq-cooling cpufreq-cooling cpu-hotplug userspace-cdev qmi-cooling bcl-soc bcl-pmic; do one $t; done
one task-vendor-data "avr_expect_vendor=512 android_arch_task_struct_size=512"
for t in sched-walt thermal-pause moto-sched; do one $t "android_arch_task_struct_size=512"; done
one dma-heaps
one qcom-tee
one qcom-reboot
one qcom-cpucp-scmi
one firststage-misc
one qcom-debug-diag
one qcom-perf-misc
one qcom-pm
one qcom-usb
one qcom-charging
one intree-misc
one mm-drivers
one msm-drm
one audio-conn
one media-data
one soc-misc
one wlan
python3 scripts/audit_module_versions.py ports/*/*.ko 2>/dev/null | python3 -c "import json,sys;d=json.load(sys.stdin);print('crc',d['passed'],len(d['modules']),[m['module'] for m in d['modules'] if m['mismatches']][:6])"
