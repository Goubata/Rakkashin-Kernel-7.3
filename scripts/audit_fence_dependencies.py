#!/usr/bin/env python3
"""Inventory real staged fence objects; this does not prove final module linkage."""
from pathlib import Path
import importlib.util
import json

ROOT=Path(__file__).resolve().parents[1]
spec=importlib.util.spec_from_file_location('module_inventory',ROOT/'scripts/inventory_symbols.py')
inventory=importlib.util.module_from_spec(spec)
spec.loader.exec_module(inventory)
base=ROOT/'ports/qti-fence'
objects={name:inventory.inspect((base/name).read_bytes()) for name in
         ['synx-driver.o','ipclite.o','msm_hw_fence.o']}
tables=[ROOT/'out/native-gki/Module.symvers']
tables += [ROOT/'ports'/name/'Module.symvers' for name in
           ['secure-buffer','qcom-smem','qcom-hwspinlock','qcom-rproc-common','gunyah']]
exports={s for info in objects.values() for s in info['exports']}
for table in tables:
    exports.update(line.split()[1] for line in table.read_text().splitlines() if line.strip())
results={name:{'import_count':len(info['imports']),
               'unresolved':sorted(set(info['imports'])-exports-{'__this_module'})}
         for name,info in objects.items()}
report={'scope':'object import name coverage; final modules and hardware not verified',
        'objects':results,
        'unresolved':sorted({s for info in results.values() for s in info['unresolved']}),
        'required_providers':['Gunyah resource manager'],
        'soccp_variant':'AVR V2 queue/IPCC/firmware acknowledgement retained; unsupported V1 rejected without its provider'}
(ROOT/'reports/qti-fence-dependencies.json').write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
