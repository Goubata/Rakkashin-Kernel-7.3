#!/usr/bin/env python3
"""Build native-derived BCL SoC drafts and optional private-RAM tests; no phone IO."""
from pathlib import Path
import argparse,shlex,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--tests',action='store_true');a=p.parse_args()
for name in ['audit_core_patch_replay.py','audit_bcl_soc_sources.py']:
 subprocess.run([sys.executable,str(ROOT/'scripts'/name)],check=True)
if a.tests:
 raw=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/android-7.3'),'show','4dd306a07b2f456fd70ddaa28f4f65640374f202:kernel/power/qos.c']).decode()
 snippets=[]
 for name in ['pm_qos_read_value','freq_constraints_init','freq_qos_read_value']:
  start=raw.index(('void ' if name=='freq_constraints_init' else 's32 ')+name+'(')
  end=raw.index('\n}',start)+2
  snippets.append('static '+raw[start:end])
 generated='\n\n'.join(snippets)
 for name in ['pm_qos_read_value','freq_constraints_init','freq_qos_read_value']:
  generated=generated.replace(name+'(', 'local_'+name+'(')
 (ROOT/'tests/bcl-soc/native-qos-test.h').write_text('/* Exact pinned native nonexported QoS init/read methods, local names. */\n'+generated+'\n')
for target in ['ports/qcom-bcl-soc']+(['tests/bcl-soc'] if a.tests else []):
 command=['chroot',str(ROOT.parent/'.andronix/distros/ubuntu/rootfs'),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j2','modules']
 subprocess.run(['su','-c',shlex.join(command)],check=True)
