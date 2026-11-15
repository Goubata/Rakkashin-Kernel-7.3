#!/usr/bin/env python3
"""Replay exact pinned native TSENS source and reviewed port patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,re,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/android-7.3'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');args=p.parse_args()
r=json.loads((ROOT/'reports/tsens-source-inputs.json').read_text());assert r['source_commit']=='4dd306a07b2f456fd70ddaa28f4f65640374f202'
assert r['factory_reference_commit']=='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
assert r['factory_reference_file']=='drivers/thermal/qcom/tsens-v2.c'
factory=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/qcom-soc'),'show',r['factory_reference_commit']+':'+r['factory_reference_file']])
feature=re.search(r'static struct tsens_features tsens_v2_feat = \{(.*?)\n\};',factory.decode(),re.S).group(1)
assert int(re.search(r'\.trip_max_temp\s*=\s*(\d+)',feature).group(1))==204000
files={**r['files'],'ports/qcom-tsens/Makefile':None}
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',r['source_commit']+':'+src]) if src else b'' for n,src in files.items()}
def sha(data):return hashlib.sha256(data).hexdigest()
patch=ROOT/'patches/tsens-7.3.patch';lock=ROOT/'reports/tsens-source-lock.json'
if args.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':r['source_commit'],'factory_reference_sha256':sha(factory),'files':rows,'patch_sha256':sha(patch.read_bytes()),'experimental_packaged':False,'physical_verified':False,'scope':'Source/patch replay only; unaccepted TSENS driver draft','reviewed_changes':['Use native 7.3 generic TSENS v2 matching saved AVR rather than the legacy 26xx family', 'External Kbuild include path only; native thermal APIs/regmap/IRQ/devres retained', 'Bound DT sensor counts by the native field limit and explicit hardware-ID table length', 'Preserve factory generic-v2 204000 mC upper threshold range; no DT trip values changed', 'Return register errors from IRQ mask helpers; mask both sides while setting a threshold window and report the first failure with best-effort masking'],'pending':['Physical calibration/readings, IRQ thresholds/watchdog, firmware and suspend/remove acceptance', 'Remaining IRQ-handler/register-error paths, persistent bus faults and complete probe/unbind tests', 'Factory persistent-temperature diagnostics are not provided by the native driver', 'Whole-board driver/DT/boot/vendor_boot/DLKM/SELinux acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['factory_reference_sha256']==sha(factory);assert l['source_commit']==r['source_commit'] and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/tsens-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified pinned native TSENS patch replay:',len(files),'files; unaccepted drafts')
