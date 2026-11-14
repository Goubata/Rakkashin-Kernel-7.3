#!/usr/bin/env python3
"""Materialize only the pinned AVR DTS include graph into ignored artifacts."""
from pathlib import Path,PurePosixPath
import argparse
import hashlib
import json
import posixpath
import os
import re
import subprocess

ROOT=Path(__file__).resolve().parents[1]
AUDIT=ROOT.parent/'avr-upstream-audit'
parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--components',action='store_true',help='Also materialize AVR audio/display/camera input graphs for research')
parser.add_argument('--component',action='append',choices=['display','audio','camera','gpu','hw-fence','hfi','synx','video','data','bluetooth','wlan','nfc','dsp','eva'],default=[])
parser.add_argument('--camera-reference',action='store_true',help='Explicitly use the unverified Vantage camera binding reference in a separate research bundle')
parser.add_argument('--extended-platform',action='store_true',help='Add firmware-pinned GPU, fence, video, data, BT and WLAN graphs for research')
parser.add_argument('--dsp-eva-reference',action='store_true',help='Use explicit Alor DSP/EVA references from different Motorola releases, without assuming firmware compatibility')
args=parser.parse_args()
if args.dsp_eva_reference and not args.extended_platform:parser.error('--dsp-eva-reference requires --extended-platform')
selected=set(['display','audio','camera'] if args.components else args.component)
if args.extended_platform:
 if not args.camera_reference or not {'display','audio','camera'}<=selected:parser.error('--extended-platform requires --components --camera-reference')
 selected |= {'gpu','hw-fence','hfi','synx','video','data','bluetooth','wlan','nfc'}
if selected & {'dsp','eva'} and not args.dsp_eva_reference:parser.error('DSP/EVA selections require --dsp-eva-reference')
if args.dsp_eva_reference:selected |= {'dsp','eva'}
args.components=bool(selected)
if args.camera_reference and 'camera' not in selected:parser.error('--camera-reference requires selecting camera components')
DEST=ROOT/('artifacts/dt-component-source-bundle' if args.components else 'artifacts/dt-source-bundle')
REPORT=ROOT/('reports/dt-component-source-lock.json' if args.components else 'reports/dt-source-lock.json')
if args.camera_reference:
 DEST=ROOT/'artifacts/dt-camera-reference-bundle'
 REPORT=ROOT/'reports/dt-camera-reference-lock.json'
if args.extended_platform:
 DEST=ROOT/'artifacts/dt-avr-platform-bundle'
 REPORT=ROOT/'reports/dt-avr-platform-source-lock.json'
if args.dsp_eva_reference:
 DEST=ROOT/'artifacts/dt-avr-dsp-eva-reference-bundle'
 REPORT=ROOT/'reports/dt-avr-dsp-eva-reference-source-lock.json'
rows=json.loads((ROOT/'reports/vendor-source-refs.json').read_text())
repos={r['repository']:(AUDIT/'vendor-repos'/r['repository'],r['commit']) for r in rows
       if ('devicetree' in r['repository'] or r['repository'] in ('motorola-kernel-modules','vendor-qcom-opensource-audio-kernel')) and 'commit' in r}
repos['qcom-soc']=(AUDIT/'qcom-soc','037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1')
repos['kernel-common']=(AUDIT/'vendor-repos/kernel-common','479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4')
if args.camera_reference:
 repos['camera-vantage-reference']=(AUDIT/'camera-vantage-reference','cfc9f5178968206a4c859e29350222b68ca1cd13')
if args.extended_platform:
 repos['avr-dt-compat']=(ROOT,'939f0f35270aaa103b4f9dfeb7e98fb3aa3d8918')
if args.dsp_eva_reference:
 repos['kernel-dsp-reference']=(AUDIT/'kernel-dsp-devicetree-reference','a6c46c4e0203f4679ac7249a1e5d30a7fedfad3c')
 repos['kernel-eva-reference']=(AUDIT/'kernel-eva-devicetree-reference','90082d47f3259fe1888019f8a6e78dffa47f24ad')
indexes={}
links={}
aliases={}
for name,(repo,commit) in repos.items():
 paths = ['dt-bindings'] if name=='camera-vantage-reference' else ['include'] if name in ('qcom-soc', 'kernel-common', 'vendor-qcom-opensource-audio-kernel') else (['drivers'] if name == 'motorola-kernel-modules' else [])
 if name=='avr-dt-compat':paths=['ports/device-tree/compat']
 data=subprocess.check_output(['git','-C',str(repo),'ls-tree','-rz','--full-tree',commit]+(['--']+paths if paths else []))
 entries={};symlinks={}
 for entry in data.split(b'\0'):
  if not entry:continue
  meta,path=entry.split(b'\t',1);mode,kind,sha=meta.decode().split()
  if kind=='blob' and mode in ('100644','100755'):entries[path.decode()]=sha
  if kind=='blob' and mode=='120000':symlinks[path.decode()]=sha
 indexes[name]=entries
 links[name]=symlinks

def follow(name,path,depth=0):
 if depth>16:raise ValueError('symlink chain too deep')
 if path not in links[name]:return name,path
 repo,commit=repos[name]
 target=subprocess.check_output(['git','-C',str(repo),'show',commit+':'+path],timeout=90).decode().strip()
 local=posixpath.normpath(posixpath.join(posixpath.dirname(path),target))
 candidates=[]
 if local in indexes[name] or local in links[name]:candidates.append((name,local))
 for marker,other in [('motorola/kernel/modules/','motorola-kernel-modules')]+[(n.removeprefix('kernel-')+'/',n) for n in repos if 'devicetree' in n]:
  if marker in target:
   tail=target.split(marker,1)[1]
   if tail in indexes[other] or tail in links[other]:candidates.append((other,tail))
 candidates=list(dict.fromkeys(candidates))
 if len(candidates)!=1:raise ValueError(f'need symlink mapping for {name}:{path} -> {target}')
 result=follow(*candidates[0],depth+1)
 aliases[name+':'+path]={'source_git_blob':links[name][path],'target':result[0]+':'+result[1]}
 return result

def resolve(name,path,include):
 local=posixpath.normpath(posixpath.join(posixpath.dirname(path),include))
 if local in indexes[name] or local in links[name]:return follow(name,local)
 if include.startswith('dt-bindings/'):
  for repo in ['qcom-soc','kernel-common']:
   key='include/'+include
   if key in indexes[repo] or key in links[repo]:return follow(repo,key)
  if args.camera_reference and include=='dt-bindings/msm-camera.h':
   return follow('camera-vantage-reference',include)
  if args.extended_platform and include=='dt-bindings/ipclite-signals.h':
   return follow('avr-dt-compat','ports/device-tree/compat/'+include)
  raise ValueError('missing firmware binding '+include)
 candidates=[]
 for repo,tree in indexes.items():
  for key in [include,'qcom/'+include,'include/'+include]:
   if key in tree or key in links[repo]:candidates.append(follow(repo,key))
 candidates=list(dict.fromkeys(candidates))
 same=[c for c in candidates if c[0]==name]
 if len(same)==1:return same[0]
 if len(candidates)==1:return candidates[0]
 if not candidates:
  suffix='/'+include
  candidates=[(repo,key) for repo,tree in indexes.items() for key in tree if key.endswith(suffix)]
  same=[c for c in candidates if c[0]==name]
  if len(same)==1:return same[0]
  if len(candidates)==1:return candidates[0]
 raise ValueError(f'include resolution needs review: {name}:{path} -> {include}: {candidates[:8]}')

roots=[('kernel-devicetree','qcom/alor-avr-base.dts'),
       ('kernel-devicetree','qcom/alor-avr-evb-overlay.dtso')]
component_roots={
 'display':[('kernel-display-devicetree','display/alor-sde.dts'),
            ('kernel-display-devicetree','display/alor-sde-display-avr-evb-overlay.dts')],
 'camera':[('kernel-camera-devicetree','alor-camera.dts'),
           ('kernel-camera-devicetree','alor-camera-sensor-avr-evb.dts')],
 'audio':[('kernel-audio-devicetree','alor-audio.dts'),
          ('kernel-audio-devicetree','alor-audio-moto-avr-evb.dts')]}
component_roots.update({
 'gpu':[('kernel-graphics-devicetree','gpu/alor-gpu.dts')],
 'hw-fence':[('kernel-mm-devicetree','hw_fence/alor-hw-fence.dts')],
 'hfi':[('kernel-mm-devicetree','hfi_core/alor-hfi-core.dts')],
 'synx':[('kernel-synx-devicetree','synx/canoe-synx.dts')],
 'video':[('kernel-video-devicetree','alor-vidc.dts')],
 'data':[('kernel-data-devicetree','alor-ipa.dts'),('kernel-data-devicetree','alor-smem-mailbox.dts')],
 'bluetooth':[('kernel-bt-devicetree','alor-wcn7750-bt-avr-evb.dts')],
 'wlan':[('kernel-wlan-devicetree','alor-qrd-wcn7750-avr-evb.dts')],
 'nfc':[('kernel-nfc-devicetree','nxp/alor-nfc-avr-evb.dts')],
 'dsp':[('kernel-dsp-reference','alor/alor-dsp.dts')],
 'eva':[('kernel-eva-reference','alor-eva.dts')]})
for component in sorted(selected):roots += component_roots[component]
queue=list(roots);seen=set();files=[];edges=[]
DEST.mkdir(parents=True,exist_ok=True)
try:
 while queue:
  name,path=queue.pop(0)
  if (name,path) in seen:continue
  if '..' in PurePosixPath(path).parts or PurePosixPath(path).is_absolute():raise ValueError('unsafe source path')
  sha=indexes[name][path];repo,commit=repos[name]
  target=DEST/name/path
  data=None
  if target.is_file():
   cached=target.read_bytes()
   if hashlib.sha1(b'blob '+str(len(cached)).encode()+b'\0'+cached).hexdigest()==sha:data=cached
  if data is None:
   data=subprocess.check_output(['git','-C',str(repo),'show',commit+':'+path],timeout=90)
  if hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()!=sha:raise ValueError('blob hash mismatch')
  target.parent.mkdir(parents=True,exist_ok=True);target.write_bytes(data);seen.add((name,path))
  files.append({'repository':name,'commit':commit,'path':path,'git_blob':sha,'sha256':hashlib.sha256(data).hexdigest()})
  text=data.decode('utf-8')
  for include in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]',text,re.M):
   resolved=resolve(name,path,include);queue.append(resolved)
   edges.append({'from':name+':'+path,'include':include,'to':resolved[0]+':'+resolved[1]})
  if len(files)%20==0:print('Verified source files:',len(files),flush=True)
 report={'dsp_eva_reference_used':args.dsp_eva_reference,'dsp_eva_firmware_compatibility_verified':False,'extended_platform':args.extended_platform,'camera_reference_used':args.camera_reference,'camera_driver_compatibility_verified':False,
         'status':'include graph materialized','roots':[n+':'+p for n,p in roots],
         'repositories':{n:c for n,(_,c) in repos.items() if any(f['repository']==n for f in files)},
         'files':files,'include_edges':edges,'symlink_aliases':aliases,'scope':'pinned source inputs only; not a compiled or validated 7.3 DTB'}
except Exception as error:
 report={'status':'incomplete','error':str(error),'files':files,'include_edges':edges}
 REPORT.write_text(json.dumps(report,indent=2)+'\n')
 raise
for alias,entry in aliases.items():
 name,path=alias.split(':',1);other,tail=entry['target'].split(':',1)
 link=DEST/name/path;target=DEST/other/tail
 link.parent.mkdir(parents=True,exist_ok=True)
 if link.is_symlink():link.unlink()
 elif link.exists():raise ValueError('source symlink would overwrite regular file')
 link.symlink_to(os.path.relpath(target,link.parent))
REPORT.write_text(json.dumps(report,indent=2)+'\n')
print('Completed AVR include graph:',len(files),'files',flush=True)
