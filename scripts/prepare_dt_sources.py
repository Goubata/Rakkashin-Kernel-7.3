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
parser.add_argument('--component',action='append',choices=['display','audio','camera'],default=[])
args=parser.parse_args()
selected=set(['display','audio','camera'] if args.components else args.component)
args.components=bool(selected)
DEST=ROOT/('artifacts/dt-component-source-bundle' if args.components else 'artifacts/dt-source-bundle')
REPORT=ROOT/('reports/dt-component-source-lock.json' if args.components else 'reports/dt-source-lock.json')
rows=json.loads((ROOT/'reports/vendor-source-refs.json').read_text())
repos={r['repository']:(AUDIT/'vendor-repos'/r['repository'],r['commit']) for r in rows
       if ('devicetree' in r['repository'] or r['repository'] in ('motorola-kernel-modules','vendor-qcom-opensource-audio-kernel')) and 'commit' in r}
repos['qcom-soc']=(AUDIT/'qcom-soc','037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1')
repos['kernel-common']=(AUDIT/'vendor-repos/kernel-common','479a9e4aef5cc78d6c5443bfb06be8ef09bdebc4')
indexes={}
links={}
aliases={}
for name,(repo,commit) in repos.items():
 paths = ['include'] if name in ('qcom-soc', 'kernel-common', 'vendor-qcom-opensource-audio-kernel') else (['drivers'] if name == 'motorola-kernel-modules' else [])
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
 report={'status':'include graph materialized','roots':[n+':'+p for n,p in roots],
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
