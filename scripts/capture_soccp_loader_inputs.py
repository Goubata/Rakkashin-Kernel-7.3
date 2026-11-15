#!/usr/bin/env python3
"""Copy the two already-audited mounted SoCCP files to local test artifacts only."""
from pathlib import Path
import hashlib
import json
ROOT=Path(__file__).resolve().parents[1]
record=json.loads((ROOT/'reports/soccp-firmware-stage40-validation.json').read_text())
folder=ROOT/'artifacts/soccp-loader-inputs-stage41';folder.mkdir(parents=True,exist_ok=True)
rows=[]
for name in ['soccp.mbn','soccp_dtb.mbn']:
 source=Path('/vendor/soccp_firmware/image')/name
 expected=next(v for v in record['files'] if v['file']==str(source))['sha256']
 raw=source.read_bytes()
 if hashlib.sha256(raw).hexdigest()!=expected:raise ValueError('Live firmware differs from reviewed input: '+name)
 target=folder/name
 if target.exists():
  if target.read_bytes()!=raw:raise ValueError('Existing captured input differs: '+name)
 else:
  with target.open('xb') as stream:stream.write(raw)
  target.chmod(0o644)
 rows.append({'file':str(target.relative_to(ROOT)),'source':str(source),'sha256':expected,'size':len(raw)})
print(json.dumps({'scope':'copies of audited mounted firmware for isolated RAM loader tests only','partition_written':False,'files':rows},indent=2))
