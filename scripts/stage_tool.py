#!/usr/bin/env python3
"""Per-stage helpers that replace repeated manual edits; never touches the phone.

register  NAME --modules a.ko,b.ko --rmmod x,y --dtso tests/NAME/f.dtso --cases N --marker SCOPE
          Adds one KUnit QEMU test to run_qemu.py, make_qemu_initramfs.py and check_qemu_log.py
          using the same anchors the existing entries use (idempotent).
build     PORT [TEST]   Run external module builds in the pinned chroot (root required).
qemu      NAME [--cpus 4]  Run the QEMU test, then the log checker; non-zero on failure.
artifact  STAGE NAME PREFIX --files f1,f2 [--dtb]  Copy outputs into artifacts/PREFIX-draft-stageSTAGE,
          write metadata.json/SHA256SUMS and reports/PREFIX-stageSTAGE-crc.json for the .ko files.
"""
from pathlib import Path
import argparse,hashlib,json,shlex,shutil,subprocess,sys
ROOT=Path(__file__).resolve().parents[1]
ROOTFS=ROOT.parent/'.andronix/distros/ubuntu/rootfs'
IMAGE27='2555cbe41505b155ef31f9a01f98e162209fcf132bce2ae936e32e789474143f'

def replace_once(s,old,new,path):
 if old not in s:raise SystemExit(f'{path}: anchor not found: {old[:60]}')
 return s.replace(old,new,1)

def register(a):
 n=a.name;flag=n.replace('-','_')
 p=ROOT/'scripts/check_qemu_log.py';s=p.read_text()
 if f"'{n}'," not in s.split('choices=[',1)[1].split(']',1)[0]:
  s=replace_once(s,"choices=[",f"choices=['{n}',",p)
  s=replace_once(s,"markers={",f"markers={{'{n}':'{a.marker_line}',",p)
  i=s.index('expected=')
  if_line_start=s.rindex('\n',0,s.rindex('\n',0,i))+1
  if_line=s[if_line_start:s.index('\n',if_line_start)]
  s=s[:if_line_start]+if_line.replace("if a.test in (",f"if a.test in ('{n}',",1)+s[s.index('\n',if_line_start):]
  s=replace_once(s,"expected=",f"expected={a.cases} if a.test=='{n}' else ",p)
  p.write_text(s)
 p=ROOT/'scripts/run_qemu.py';s=p.read_text()
 if f"'{n}':'{n}-tests'" not in s:
  s=replace_once(s,"choices=[",f"choices=['{n}',",p)
  s=replace_once(s,"ramdisk={",f"ramdisk={{'{n}':'{n}-tests',",p)
  if a.dtso:
   idx=s.index("    folder=ROOT/'artifacts'/('qemu-'+a.test)")
   line_start=s.rindex('\n',0,idx-1)+1
   s=s[:line_start]+s[line_start:idx].replace("if a.test in (",f"if a.test in ('{n}',",1)+s[idx:]
   rel=Path(a.dtso).relative_to('tests').as_posix()
   s=replace_once(s,"overlay='/work/avr-port/tests/'+(",f"overlay='/work/avr-port/tests/'+('{rel}' if a.test=='{n}' else ",p)
  p.write_text(s)
 p=ROOT/'scripts/make_qemu_initramfs.py';s=p.read_text()
 if f'"--{n}-tests"' not in s:
  first=s.index('    group.add_argument("--')
  s=s[:first]+f'    group.add_argument("--{n}-tests", action="store_true")\n'+s[first:]
  mods=[m for m in a.modules.split(',') if m]
  srcs=",".join(["ROOT/'out/native-gki/lib/kunit/kunit.ko'"]+[f"ROOT/'{m}'" for m in mods])
  up=n.upper().replace('-','_')
  block=f'''    if args.{flag}_tests:
        sources=[{srcs}]
        commands=[]
        for source in sources:
            add('modules/'+source.name,stat.S_IFREG|0o644,source.read_bytes())
            params=' enable=1' if source.name=='kunit.ko' else ''
            commands.append(f'/bin/busybox insmod /modules/{{source.name}}{{params}} || {{{{ echo AVR_{up}_LOAD_FAIL; /bin/busybox poweroff -f; }}}}')
        for module in {a.rmmod.split(',')!r}:
            commands.append(f'/bin/busybox rmmod {{module}} || {{{{ echo AVR_{up}_UNLOAD_FAIL; /bin/busybox poweroff -f; }}}}')
        commands.append('echo {a.marker_line}')
        init=init.replace(b'echo AVR_QEMU_SMOKE_PASS','\\n'.join(commands).encode())
'''
  anchor=s.index("    if args.devfreq_cooling_tests:") if "    if args.devfreq_cooling_tests:" in s else s.index("    if args.ddr_cooling_tests:")
  s=s[:anchor]+block+s[anchor:]
  s=replace_once(s,"target=ROOT/'tests/qemu'/(",f"target=ROOT/'tests/qemu'/('{n}-tests.cpio.gz' if args.{flag}_tests else ",p)
  p.write_text(s)
 print('registered',n)

def chroot_make(target):
 cmd=['chroot',str(ROOTFS),'/usr/bin/env','-i','PATH=/usr/sbin:/usr/bin:/sbin:/bin','make','-C','/work/avr-src','O=/work/avr-port/out/native-gki','ARCH=arm64','LLVM=1','M=/work/avr-port/'+target,'-j4','modules']
 subprocess.run(['su','-c',shlex.join(cmd)],check=True)

def build(a):
 for t in [a.port]+([a.test] if a.test else []):chroot_make(t)

def qemu(a):
 log=ROOT/'reports'/(a.log or f'{a.name}-qemu.log')
 subprocess.run([sys.executable,str(ROOT/'scripts/run_qemu.py'),'--test',a.name,'--cpus',str(a.cpus),'--log',str(log)],check=False)
 r=subprocess.run([sys.executable,str(ROOT/'scripts/check_qemu_log.py'),'--test',a.name,str(log)],capture_output=True,text=True)
 print(r.stdout[-600:]);raise SystemExit(r.returncode)

def artifact(a):
 d=ROOT/'artifacts'/f'{a.prefix}-draft-stage{a.stage}';d.mkdir(parents=True,exist_ok=True)
 files=[ROOT/f for f in a.files.split(',') if f]
 for f in files:shutil.copy2(f,d/f.name)
 if a.dtb:shutil.copy2(ROOT/'artifacts'/f'qemu-{a.name}'/'virt-rail.dtb',d/f'virt-{a.name}.dtb')
 img=hashlib.sha256((ROOT/'out/native-gki/arch/arm64/boot/Image').read_bytes()).hexdigest()
 kos=[f for f in files if f.suffix=='.ko']
 crc=subprocess.run([sys.executable,str(ROOT/'scripts/audit_module_versions.py')]+[str(k) for k in kos],capture_output=True,text=True)
 (ROOT/'reports'/f'{a.prefix}-stage{a.stage}-crc.json').write_text(crc.stdout)
 meta={'scope':a.scope,'core_image':'7.3.0-4k #27' if img==IMAGE27 else 'changed','image_sha256':img,'modules':[k.name for k in kos if 'test' not in k.name],'test_modules':[k.name for k in kos if 'test' in k.name],'physical_verified':False,'flashable_boot_image':False,'promoted':False,'main_bundle_unchanged':'artifacts/android73-info-stage57','phone_dt_modified':False}
 (d/'metadata.json').write_text(json.dumps(meta,indent=2)+'\n')
 (d/'SHA256SUMS').write_text(''.join(f'{hashlib.sha256(p.read_bytes()).hexdigest()}  {p.name}\n' for p in sorted(d.iterdir()) if p.name!='SHA256SUMS'))
 print(d.relative_to(ROOT),'crc_passed=',json.loads(crc.stdout)['passed'])
 raise SystemExit(crc.returncode)

p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter);sp=p.add_subparsers(dest='cmd',required=True)
r=sp.add_parser('register');r.add_argument('name');r.add_argument('--modules',required=True);r.add_argument('--rmmod',required=True);r.add_argument('--dtso');r.add_argument('--cases',type=int,required=True);r.add_argument('--marker',required=True)
b=sp.add_parser('build');b.add_argument('port');b.add_argument('test',nargs='?')
q=sp.add_parser('qemu');q.add_argument('name');q.add_argument('--cpus',type=int,default=4);q.add_argument('--log')
t=sp.add_parser('artifact');t.add_argument('stage',type=int);t.add_argument('name');t.add_argument('prefix');t.add_argument('--files',required=True);t.add_argument('--dtb',action='store_true');t.add_argument('--scope',default='Unaccepted draft; private QEMU fixture only')
a=p.parse_args()
if a.cmd=='register':a.marker_line=f"AVR_{a.name.upper().replace('-','_')}_TESTS_PASS scope={a.marker}"
{'register':register,'build':build,'qemu':qemu,'artifact':artifact}[a.cmd](a)
