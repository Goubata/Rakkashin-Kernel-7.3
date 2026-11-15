#!/usr/bin/env python3
"""Replay exact pinned native Qualcomm RNG source and reviewed port patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/android-7.3'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');args=p.parse_args()
r=json.loads((ROOT/'reports/rng-source-inputs.json').read_text());assert r['source_commit']=='4dd306a07b2f456fd70ddaa28f4f65640374f202'
files={**r['files'],'ports/qcom-rng/Makefile':None}
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',r['source_commit']+':'+src]) if src else b'' for n,src in files.items()}
def sha(data):return hashlib.sha256(data).hexdigest()
patch=ROOT/'patches/rng-7.3.patch';lock=ROOT/'reports/rng-source-lock.json'
if args.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':r['source_commit'],'files':rows,'patch_sha256':sha(patch.read_bytes()),'experimental_packaged':False,'physical_verified':False,'scope':'Source/patch replay only; unaccepted hardware RNG draft','reviewed_changes':['Build the pinned native 7.3 qcom,trng driver externally with native hwrng and optional clock APIs', 'Honor nonblocking reads without sleeping, leave zero-length requests inert and return already copied bytes when a later FIFO wait fails', 'Bound the size_t callback request by INT_MAX before narrowing; reject a nonempty NULL buffer'],'pending':['Real entropy source/firmware/health and byte-stream quality; no private-RAM data fed to the kernel entropy pool', 'Physical hwrng registration, clock ownership, probe/remove and suspend/early boot behavior', 'All remaining drivers and whole-board DT/boot/DLKM/SELinux acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==r['source_commit'] and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/rng-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified pinned native Qualcomm RNG patch replay:',len(files),'files; unaccepted drafts')
