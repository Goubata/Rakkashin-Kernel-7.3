#!/usr/bin/env python3
"""Replay pinned Qualcomm CPU hotplug cooling source and the reviewed native 7.3 port patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/qcom-soc'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');a=p.parse_args()
commit='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
files={'ports/qti-cpu-hotplug/cpu_hotplug.c':'drivers/thermal/qcom/cpu_hotplug.c','ports/qti-cpu-hotplug/Makefile':None}
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',commit+':'+src]) if src else b'' for n,src in files.items()}
def sha(b):return hashlib.sha256(b).hexdigest()
patch=ROOT/'patches/cpu-hotplug-7.3.patch';lock=ROOT/'reports/cpu-hotplug-source-lock.json'
if a.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':commit,'files':rows,'patch_sha256':sha(patch.read_bytes()),'physical_verified':False,'combined_bundle_promoted':False,'scope':'Pinned factory CPU hotplug cooling OF cdev/alias/veto semantics; native 7.3 per-device lifetime, lock order and restore port','reviewed_changes':['Keep OF cdev on each child (DT cooling maps), qcom,cdev-alias or cpu-hotplugN, max state 1, online veto while mitigated','Per-device context and multi-instance cpuhp replace global list reset by INIT_LIST_HEAD on every probe','Unregister cdevs outside callback lock (factory held cpu_hot_lock across thermal unregister that drains get_cur_state: deadlock)','Cancel reg/exec work before teardown, restore only CPUs this driver offlined, hold child OF node references','Exec work converges to the latest request; does not online a CPU the user offlined; veto returns -EBUSY instead of NOTIFY_BAD'],'pending':['Physical CPU isolation under thermal HAL/vendor hooks and suspend interaction','Whole-board drivers/DT/boot/DLKM/SELinux and daily-use acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==commit and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/cpu-hotplug-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified factory CPU hotplug cooling source replay:',len(files),'files; unaccepted draft')
