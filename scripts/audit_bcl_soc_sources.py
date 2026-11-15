#!/usr/bin/env python3
"""Replay exact pinned factory Qualcomm BCL SoC source and reviewed port patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/qcom-soc'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');args=p.parse_args()
r=json.loads((ROOT/'reports/bcl-soc-source-inputs.json').read_text());assert r['source_commit']=='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
files={**r['files'],'ports/qcom-bcl-soc/Makefile':None}
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',r['source_commit']+':'+src]) if src else b'' for n,src in files.items()}
def sha(data):return hashlib.sha256(data).hexdigest()
patch=ROOT/'patches/bcl-soc-7.3.patch';lock=ROOT/'reports/bcl-soc-source-lock.json'
if args.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':r['source_commit'],'files':rows,'patch_sha256':sha(patch.read_bytes()),'experimental_packaged':False,'physical_verified':False,'scope':'Pinned source and patch replay only; unaccepted BCL SoC draft','reviewed_changes':['Per-device power-supply references, notifier/work and thermal registration ownership with native 7.3 APIs','Preserve depletion metric and boot policy with strict property validation and error propagation','One owned QoS request/reference per shared CPU policy; rollback partial setup; drain work before managed zone/supply release','Use native cached thermal temperature for trend without recursive sensor reads'],'pending':['Physical battery/USB/thermal/cooling behavior and actual OF registration','Supplier driver forced unbind, late supplier deferred-probe wakeup, exhaustive PM/unbind races','Whole-board drivers/DT/boot/DLKM/SELinux acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==r['source_commit'] and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/bcl-soc-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified pinned factory Qualcomm BCL SoC patch replay:',len(files),'files; unaccepted drafts')
