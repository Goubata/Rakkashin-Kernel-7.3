#!/usr/bin/env python3
"""Inventory GPU object imports against the provider tables actually used by Kbuild."""
from pathlib import Path
import importlib.util
import json
import re

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('module_inventory',ROOT/'scripts/inventory_symbols.py')
inventory=importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)
source=ROOT/'ports/kgsl'
tables=[ROOT/'out/native-gki/Module.symvers']
tables += [(source/p).resolve() for p in re.findall(r'\$\(src\)/(\.\./[^\s]+/Module\.symvers)',(source/'Kbuild').read_text())]
exports=set()
for table in tables:
    for line in table.read_text().splitlines():
        fields=line.split()
        if fields:exports.add(fields[1])
info=inventory.inspect((source/'msm_kgsl.o').read_bytes())
missing=sorted(set(info['imports'])-exports-{'__this_module'})
report={
    'scope':'object import name coverage only; final .ko and hardware operation not proved',
    'import_count':len(info['imports']),'unresolved_count':len(missing),
    'unresolved':missing,'provider_tables':[str(p.relative_to(ROOT)) for p in tables],
    'excluded_module_loader_symbols':['__this_module'],
}
(ROOT/'reports/kgsl-link-dependencies.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
