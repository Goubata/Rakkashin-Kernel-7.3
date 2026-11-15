#!/usr/bin/env python3
"""Compile exact ESD source methods against recorded primitive faults."""
from pathlib import Path
import hashlib,json,subprocess
ROOT=Path(__file__).resolve().parents[2]
src=ROOT/'ports/goodix-touch/goodix_thp/goodix_thp.c';s=src.read_text()
def extract(name):
 a=s.index(name+'(');a=s.rfind('\n',0,a)+1;b=s.index('\n}',a)+2;return s[a:b]
probe=s[s.index('static int goodix_thp_probe('):s.index('static void goodix_thp_remove(')]
assert probe.count('goodix_thp_esd_init(core_data)')==1
assert probe.index('goodix_thp_esd_init(core_data)')<probe.index('misc_register(')
fixture=(Path(__file__).parent/'fixture.c').read_text().replace('@FUNCTIONS@',extract('goodix_thp_esd_work')+'\n'+extract('goodix_thp_esd_init'))
out=ROOT/'artifacts/goodix-esd-stage62';out.mkdir(parents=True,exist_ok=True);(out/'test.c').write_text(fixture)
subprocess.run(['clang','-std=gnu11','-Wall','-Wextra','-Werror','-O1','-ftrivial-auto-var-init=pattern',str(out/'test.c'),'-o',str(out/'test')],check=True)
r=subprocess.run([str(out/'test')],check=True,capture_output=True,text=True)
report={'scope':'Selected ESD source methods with recorded read/write/power/work primitives and endpoint publication-order assertion; not native workqueue concurrency or physical ESD proof','passed':True,'output':r.stdout.strip(),'init_before_misc_publication':True,'source_sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'fixture_sha256':hashlib.sha256(fixture.encode()).hexdigest()}
(ROOT/'reports/goodix-esd-stage62-tests.json').write_text(json.dumps(report,indent=2)+'\n');print(r.stdout,end='')
