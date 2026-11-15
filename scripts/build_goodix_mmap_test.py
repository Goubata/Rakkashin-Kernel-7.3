#!/usr/bin/env python3
"""Build exact source mapping/release methods and static native VM guest test."""
from pathlib import Path
import subprocess,shlex
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'ports/goodix-touch/goodix_thp/goodix_thp.c').read_text()
def extract(name):
 a=s.index(name+'(');a=s.rfind('\n',0,a)+1;b=s.index('\n}',a)+2;return s[a:b]
(ROOT/'tests/goodix-mmap/generated.h').write_text('/* Generated exact production methods. */\n'+extract('goodix_thp_frame_buffer_release')+'\n'+extract('goodix_thp_mmap')+'\n')
base=['chroot',str(ROOT.parent/'.andronix/distros/ubuntu/rootfs'),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin']
for command in [base+['make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/tests/goodix-mmap','-j2','modules'],base+['clang','-static','-O2','-Wall','-Wextra','-Werror','/work/avr-port/tests/goodix-mmap/user.c','-o','/work/avr-port/tests/goodix-mmap/user-test']]:
 subprocess.run(['su','-c',shlex.join(command)],check=True)
