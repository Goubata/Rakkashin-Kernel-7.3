#!/usr/bin/env python3
"""Build watchdog/SDEI drafts and optional private-RAM tests; no phone IO."""
from pathlib import Path
import argparse,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tests',action='store_true');a=p.parse_args()
for name in ['audit_core_patch_replay.py','audit_watchdog_sources.py']:
 subprocess.run([sys.executable,str(ROOT/'scripts'/name)],check=True)
if a.tests:
 s=(ROOT/'ports/qcom-watchdog/qcom_soc_wdt.c').read_text();start=s.index('struct wdt_register_map');end=s.index('static int qcom_soc_wdt_probe(')
 (ROOT/'tests/watchdog/generated-mmio.h').write_text('/* Exact production register helpers and data. */\n'+s[start:end])
 repo=ROOT.parent/'avr-upstream-audit/android-7.3';s=(repo/'drivers/firmware/qcom/qcom_scm.c').read_text();start=s.index('int qcom_scm_sec_wdog_deactivate(');end=s.index('\n}',start)+2
 (ROOT/'tests/watchdog/generated-scm.h').write_text('/* Exact production native secure-disable method. */\n'+s[start:end]+'\n')
for target in ['ports/qcom-watchdog']+(['tests/watchdog'] if a.tests else []):
 command=['chroot',str(ROOT.parent/'.andronix/distros/ubuntu/rootfs'),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j2','modules']
 subprocess.run(['su','-c',shlex.join(command)],check=True)
