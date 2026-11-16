#!/usr/bin/env python3
"""Generic pinned-source replay for simple ports, driven by ports/<dir>/source-spec.json.

spec: {"name": "...", "repo": "qcom-soc", "commit": "...",
       "files": {"ports/<dir>/x.c": "drivers/.../x.c", "ports/<dir>/Makefile": null},
       "scope": "...", "reviewed_changes": [...], "pending": [...]}
--record rewrites patches/<name>-7.3.patch and reports/<name>-source-lock.json; the default
mode verifies that pinned originals + patch reproduce the port byte-for-byte.
"""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__,formatter_class=argparse.RawDescriptionHelpFormatter)
p.add_argument('spec',type=Path);p.add_argument('--record',action='store_true');a=p.parse_args()
spec=json.loads((ROOT/a.spec if not a.spec.is_absolute() else a.spec).read_text())
name,commit,files=spec['name'],spec['commit'],spec['files']
repo=ROOT.parent/'avr-upstream-audit'/spec.get('repo','qcom-soc')
originals={n:subprocess.check_output(['git','-C',str(repo),'show',commit+':'+src]) if src else b'' for n,src in files.items()}
def sha(b):return hashlib.sha256(b).hexdigest()
patch=ROOT/f'patches/{name}-7.3.patch';lock=ROOT/f'reports/{name}-source-lock.json'
if a.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':commit,'source_repo':spec.get('repo','qcom-soc'),'files':rows,'patch_sha256':sha(patch.read_bytes()),'physical_verified':False,'combined_bundle_promoted':False,'scope':spec['scope'],'reviewed_changes':spec['reviewed_changes'],'pending':spec['pending']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==commit and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/f'artifacts/{name}-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes()),n
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print(f'Verified {name} source replay: {len(files)} files; unaccepted draft')
