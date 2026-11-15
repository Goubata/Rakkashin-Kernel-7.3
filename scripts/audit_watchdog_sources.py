#!/usr/bin/env python3
"""Replay exact pinned Qualcomm watchdog/SDEI source and reviewed port patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/qcom-soc'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');args=p.parse_args()
r=json.loads((ROOT/'reports/watchdog-source-inputs.json').read_text());assert r['source_commit']=='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
files={**r['files'],'ports/qcom-watchdog/Makefile':None}
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',r['source_commit']+':'+src]) if src else b'' for n,src in files.items()}
def sha(data):return hashlib.sha256(data).hexdigest()
patch=ROOT/'patches/watchdog-7.3.patch';lock=ROOT/'reports/watchdog-source-lock.json'
if args.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':r['source_commit'],'files':rows,'patch_sha256':sha(patch.read_bytes()),'experimental_packaged':False,'physical_verified':False,'scope':'Source/patch replay only; unaccepted watchdog and SDEI drafts','reviewed_changes':['Use real modular minidump, SDEI and native SCM dependencies; preserve the pinned Alor reference defaults with saved DT timeout overrides', 'Use native timer APIs and preserve OF IRQ trigger configuration; reject invalid IRQ/time intervals and preserve secure-disable failure', 'Keep per-device watchdog register maps, validate MMIO error pointers/offset ranges and checked timer arithmetic', 'Avoid native SDEI name/macro collisions and propagate actual shared-reset status', 'Pin SDEI callback owner through registration and incomplete cleanup; retry EINPROGRESS, retain ownership on permanent unregister errors', 'Remove only owned watchdog minidump descriptors before freeing the allocation; retain payload memory if removal fails and use native devres ownership for normal/probe-error release', 'Unregister the PM notifier on normal remove and enable failure; clear failed watchdog publication'],'pending':['Full watchdog IRQ/thread/timer/sysfs/PM/minidump lifecycle and teardown; retained dump reclamation after backend recovery remains pending', 'Physical hypervisor/secure/PSCI/watchdog behavior and all firmware error modes', 'SDEI retained-registration recovery after failed removal and active callback drain on real firmware', 'Whole-board driver/DT/boot acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==r['source_commit'] and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/watchdog-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified pinned Qualcomm watchdog/SDEI patch replay:',len(files),'files; unaccepted drafts')
