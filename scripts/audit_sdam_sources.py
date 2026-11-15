#!/usr/bin/env python3
"""Verify factory SDAM source plus explicitly recorded bounds patch."""
import argparse
import difflib
import hashlib
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--record',action='store_true');a=p.parse_args()
r=json.loads((ROOT/'reports/sdam-source-inputs.json').read_text())
assert r['source_commit']=='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
raw=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/qcom-soc'),'show',r['source_commit']+':'+r['source']])
assert hashlib.sha256(raw).hexdigest()==r['original_sha256']
f=ROOT/'ports/qcom-sdam/qcom-spmi-sdam.c';patch=ROOT/'patches/sdam-7.3.patch';lock=ROOT/'reports/sdam-source-lock.json'
if a.record:
    patch.write_text(''.join(difflib.unified_diff(raw.decode().splitlines(True),f.read_text().splitlines(True),fromfile='a/qcom-spmi-sdam.c',tofile='b/qcom-spmi-sdam.c')))
    lock.write_text(json.dumps({**r,'staged_sha256':hashlib.sha256(f.read_bytes()).hexdigest(),'patch_sha256':hashlib.sha256(patch.read_bytes()).hexdigest(),'makefile_sha256':hashlib.sha256((f.parent/'Makefile').read_bytes()).hexdigest(),'hardware_verified':False},indent=2)+'\n')
l=json.loads(lock.read_text())
assert all(l[k]==v for k,v in r.items())
assert hashlib.sha256(f.read_bytes()).hexdigest()==l['staged_sha256']
assert hashlib.sha256(patch.read_bytes()).hexdigest()==l['patch_sha256']
assert hashlib.sha256((f.parent/'Makefile').read_bytes()).hexdigest()==l['makefile_sha256']
replay=ROOT/'artifacts/sdam-source-replay';replay.mkdir(exist_ok=True,parents=True)
(replay/f.name).write_bytes(raw)
if patch.stat().st_size:subprocess.run(['git','-C',str(ROOT),'apply','--directory='+replay.relative_to(ROOT).as_posix(),str(patch)],check=True)
assert (replay/f.name).read_bytes()==f.read_bytes()
print('Verified pinned Motorola SDAM source and explicit patch replay')
