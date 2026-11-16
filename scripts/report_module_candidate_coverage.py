#!/usr/bin/env python3
"""Count same-name ARM64 7.3 build candidates, never a functional completion rate."""
from pathlib import Path
import hashlib,json
from audit_module_versions import sections
ROOT=Path(__file__).resolve().parents[1]
base=ROOT/'device-baseline/module-inventory.json';rows=json.loads(base.read_text())
expected={r['module'].replace('-','_') for r in rows};candidates={}
early=ROOT/'device-baseline/early-modules.json'
for fragment in json.loads(early.read_text()):
 expected.update(Path(n).stem.replace('-','_') for n in fragment['files'] if n.endswith('.ko'))
paths=sorted((ROOT/'ports').rglob('*.ko'))+sorted((ROOT/'artifacts/android73-info-stage57/modules').rglob('*.ko'))
for path in paths:
 info=sections(path.read_bytes()).get('.modinfo',b'').split(b'\0')
 fields={v.split(b'=',1)[0]:v.split(b'=',1)[1] for v in info if b'=' in v}
 if not fields.get(b'vermagic',b'').startswith(b'7.3.'):continue
 name=fields.get(b'name',b'').decode().replace('-','_')
 if name in expected:candidates.setdefault(name,[]).append(path.relative_to(ROOT).as_posix())
matched=set(candidates)
print(json.dumps({'scope':'Same-name built-candidate count only, including mixed draft outputs and frozen Stage57; not one accepted package, functionality, stability or work completion','baseline_sha256':hashlib.sha256(base.read_bytes()).hexdigest(),'archived_unique_names':len(expected),'matched_candidate_names':len(matched),'name_coverage_percent':round(100*len(matched)/len(expected),1),'physical_completion_percent':None,'candidates':candidates,'missing_names':sorted(expected-matched)},indent=2))
