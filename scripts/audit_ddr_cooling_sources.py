#!/usr/bin/env python3
"""Replay pinned Qualcomm DDR cooling source and the reviewed native 7.3 port patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/qcom-soc'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');a=p.parse_args()
commit='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
files={'ports/qcom-ddr-cooling/ddr_cdev.c':'drivers/thermal/qcom/ddr_cdev.c','ports/qcom-ddr-cooling/Makefile':None}
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',commit+':'+src]) if src else b'' for n,src in files.items()}
def sha(b):return hashlib.sha256(b).hexdigest()
patch=ROOT/'patches/ddr-cooling-7.3.patch';lock=ROOT/'reports/ddr-cooling-source-lock.json'
if a.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':commit,'files':rows,'patch_sha256':sha(patch.read_bytes()),'physical_verified':False,'combined_bundle_promoted':False,'scope':'Pinned factory DDR bandwidth table and request semantics; native 7.3 resource and confirmed-state port','reviewed_changes':['Preserve factory zero-state and frequency-times-bus-width scaling without introducing a new hardware policy','Validate array cell width/count, cooling arity and nonzero width/frequencies; reject multiplication overflow','Reject a NULL optional ICC path instead of reporting successful no-op requests','Serialize callbacks, distinguish confirmed versus requested state, and retry unconfirmed requests through a drained delayed worker','Use native 7.3 managed thermal API; drain native thermal callbacks before releasing the ICC path via devres order; native icc_put releases consumer requests'],'pending':['Physical qnoc/RPMh/CRM/DDR actuation and precise frequency interpretation of the factory table','Permanent ICC release failure and exhaustive concurrent setter/worker/PM/supplier unbind recovery','Whole-board drivers/DT/boot/DLKM/SELinux and daily-use acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==commit and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/ddr-cooling-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified factory DDR cooling source replay:',len(files),'files; unaccepted draft')
