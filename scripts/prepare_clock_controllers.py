#!/usr/bin/env python3
"""Stage the firmware's loaded clock controllers and their exact DT constants."""
from pathlib import Path
import ast
import json
import re
import shutil

ROOT=Path(__file__).resolve().parents[1]
SOC=ROOT.parent/'avr-upstream-audit/qcom-soc'
DEST=ROOT/'ports/qcom-clock-controllers'

def copy_initial(source,target):
    target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists():shutil.copyfile(source,target)

def main():
    loaded={l.split()[0] for l in (ROOT/'device-baseline/loaded-modules.txt').read_text().splitlines()}
    tree=ast.parse((SOC/'drivers/clk/qcom/modules.bzl').read_text())
    chosen=[]
    DEST.mkdir(exist_ok=True)
    for node in ast.walk(tree):
        if not isinstance(node,ast.Call):continue
        kw={k.arg:k.value for k in node.keywords};out=kw.get('out')
        if not isinstance(out,ast.Constant) or not str(out.value).endswith('.ko'):continue
        name=out.value
        if name in ('clk-qcom.ko','gdsc-regulator.ko') or Path(name).stem.replace('-','_') not in loaded:continue
        paths=ast.literal_eval(kw['srcs'])
        for path in paths:copy_initial(SOC/path,DEST/Path(path).name)
        chosen.append({'module':name,'source_files':paths})
    copy_initial(ROOT/'ports/qcom-clock-core/qcom-vendor-clock.h',DEST/'qcom-vendor-clock.h')
    for rel in ['soc/qcom/crm.h','soc/qcom/minidump.h']:
        copy_initial(SOC/'include'/rel,DEST/'include'/rel)
    # A same-named upstream DT header may use different vendor clock IDs.
    # Refer to the firmware-matched constant headers by an explicit local path.
    queue=list(DEST.glob('*.c'))+list(DEST.glob('*.h'));seen=set()
    while queue:
        path=queue.pop()
        if path in seen:continue
        seen.add(path);text=path.read_text()
        def binding(match):
            rel=match.group(1);source=SOC/'include'/rel
            if not source.is_file():return match.group()
            target=DEST/'vendor-bindings'/Path(rel).relative_to('dt-bindings')
            copy_initial(source,target);queue.append(target)
            return '#include "'+str(target.relative_to(DEST))+'"'
        text=re.sub(r'#include\s+<((?:dt-bindings)/[^>]+)>',binding,text)
        if path.suffix=='.c' and '#include "qcom-vendor-clock.h"' not in text:
            includes=list(re.finditer(r'^#include[^\n]+$',text,re.M));assert includes
            at=includes[-1].end();text=text[:at]+'\n#include "qcom-vendor-clock.h"'+text[at:]
        path.write_text(text)
    make=DEST/'Makefile'
    if not make.exists():
        make.write_text('# SPDX-License-Identifier: GPL-2.0-only\nobj-m += '+' '.join(Path(i['module']).stem+'.o' for i in chosen)+'\nccflags-y += -I$(src) -I$(src)/include\nccflags-y += -DCONFIG_QCOM_CRM_V2_MODULE=1 -DCONFIG_QCOM_RPMH_MODULE=1\nKBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers\n')
    (DEST/'source-inventory.json').write_text(json.dumps({'commit':'037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1','controllers':chosen},indent=2)+'\n')
    print('Staged',len(chosen),'loaded controller modules')

if __name__=='__main__':main()
