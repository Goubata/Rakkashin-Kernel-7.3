#!/usr/bin/env python3
"""Replay exact pinned GENI DMA/I2C/SPI source and reviewed port patch."""
from pathlib import Path
import argparse,difflib,hashlib,json,shutil,subprocess
ROOT=Path(__file__).resolve().parents[1];REPO=ROOT.parent/'avr-upstream-audit/qcom-soc'
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');args=p.parse_args()
r=json.loads((ROOT/'reports/geni-bus-source-inputs.json').read_text());assert r['source_commit']=='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
files={**r['files'],'ports/geni-bus/Makefile':None}
originals={n:subprocess.check_output(['git','-C',str(REPO),'show',r['source_commit']+':'+src]) if src else b'' for n,src in files.items()}
def sha(data):return hashlib.sha256(data).hexdigest()
patch=ROOT/'patches/geni-bus-7.3.patch';lock=ROOT/'reports/geni-bus-source-lock.json'
if args.record:
 rows=[];changes=''
 for n,raw in originals.items():
  data=(ROOT/n).read_bytes();rows.append({'file':n,'source':files[n],'original_sha256':sha(raw),'sha256':sha(data)})
  changes+=''.join(difflib.unified_diff(raw.decode().splitlines(True),data.decode().splitlines(True),fromfile='a/'+n,tofile='b/'+n))
 patch.write_text(changes)
 lock.write_text(json.dumps({'source_commit':r['source_commit'],'files':rows,'patch_sha256':sha(patch.read_bytes()),'experimental_packaged':False,'physical_verified':False,'scope':'Source/patch replay only; unaccepted GENI bus driver drafts','reviewed_changes':['Keep non-exported implementation methods static and supply exact factory trace headers for external build', 'Assert equality of 35 overlapping native register constants instead of silently overriding them', 'Use the new native wrapper mapping accessor instead of casting private data to the wrong structure', 'Bound SPI chip-select scanning by the actual native array while preserving hardware four-CS limit', 'Validate ring geometry before allocation, widen multiplication and bound power-of-two rounding; reject live-ring replacement and never DMA-free SMEM backing', 'Prevalidate all SG lengths/addresses and ring capacity with subtraction before MMIO or mutation; preserve num_tre on rejection', 'Validate aligned in-ring event DMA pointers and immediate index/payload bounds before translation; preserve full 40-bit Q2SPI pointer; propagate invalid stale-scan errors', 'Detach the exact completed descriptor and complete its native DMA cookie before callback; support native result callbacks and retain private ownership across reentrant clients', 'Validate SPI rate/sampling inputs, widen target multiplication, avoid round-up overflow and enforce native divider field range', 'Initialize message DMA ownership sentinels before mapping, unwind every prior map on failure and clear each ownership token after release', 'Keep SPI master DMA channels and devres allocations private until setup succeeds; propagate all request/allocation/config/version failures and release only newly acquired resources', 'Propagate hardware prepare pinctrl failures, unwind acquired PM on setup errors and guard unprepare after failed preparation', 'Persist exact successful-message PM and sequencer-lock ownership through unprepare; balance fallback resumes and disabled-PM teardown; reject duplicate prepare and make repeated unprepare inert', 'Unwind SPI message-owned PM references and successful bus lock on prepare failure; preserve shared first-message reference handoff and remove WARN for ordinary resume errors'], 'pending':['GPI DMA rings, interrupt/completion and descriptor lifetime, remove/probe failures', 'Real I2C/SPI clock/pinctrl/firmware/DMA transactions and downstream peripheral behavior', 'Full physical device and whole-board DT/boot acceptance']},indent=2)+'\n')
l=json.loads(lock.read_text());assert l['source_commit']==r['source_commit'] and l['patch_sha256']==sha(patch.read_bytes());assert {x['file'] for x in l['files']}==set(files)
replay=ROOT/'artifacts/geni-bus-source-replay'
if replay.exists():shutil.rmtree(replay)
for row in l['files']:
 n=row['file'];assert row['original_sha256']==sha(originals[n]) and row['sha256']==sha((ROOT/n).read_bytes())
 dest=replay/n;dest.parent.mkdir(parents=True,exist_ok=True);dest.write_bytes(originals[n])
subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert all((replay/n).read_bytes()==(ROOT/n).read_bytes() for n in files)
print('Verified pinned GENI DMA/I2C/SPI patch replay:',len(files),'files; unaccepted drafts')
