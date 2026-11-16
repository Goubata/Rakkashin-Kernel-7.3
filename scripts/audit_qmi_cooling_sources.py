#!/usr/bin/env python3
"""Replay pinned Qualcomm thermal QMI protocol and the reviewed 7.3 driver patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/qcom-soc'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');a=p.parse_args()
commit='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
files={'ports/qcom-qmi-cooling/'+n:'drivers/thermal/qcom/'+n for n in ['qmi_cooling.c','thermal_mitigation_device_service_v01.c','thermal_mitigation_device_service_v01.h']}
files.update({'ports/qcom-qmi-cooling/'+n:None for n in ['qmi-cooling-main.c','Makefile']})
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',commit+':'+src]) if src else b'' for n,src in files.items()}
def sha(b):return hashlib.sha256(b).hexdigest()
patch=ROOT/'patches/qmi-cooling-7.3.patch';lock=ROOT/'reports/qmi-cooling-source-lock.json'
if a.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':commit,'files':rows,'patch_sha256':sha(patch.read_bytes()),'physical_verified':False,'combined_bundle_promoted':False,'scope':'Exact pinned protocol tables; rewritten per-device lifecycle and acknowledged state driver draft','reviewed_changes':['Native 7.3 thermal registration and unbound default workqueue','Keep desired and acknowledged states separate; return negative transport/remote errors; retry unconfirmed identical requests','Explicit peer endpoints and generation checks reject stale acknowledgements','Validate base/extended protocol list bounds, string termination and contradictory duplicates','Preserve native cdev and DT bindings across capability changes using native thermal_cooling_device_update outside state/IO locks; drain work and transactions before QMI release','Managed per-device instances and OF references; idempotent removal and partial probe unwind'],'pending':['Physical OF probe, firmware service discovery and mitigation acceptance','Exhaustive concurrent thermal setter, server replacement, suspend and teardown stress','Physical modem/CDSP replies and all 27 DT consumers','Whole-board boot/DLKM/SELinux and daily-use acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==commit and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/qmi-cooling-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified factory QMI cooling source replay:',len(files),'files; unaccepted draft')
