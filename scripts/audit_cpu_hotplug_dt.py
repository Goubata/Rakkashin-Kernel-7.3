#!/usr/bin/env python3
"""Check saved CPU hotplug cooling DT/maps against the phone snapshot; never write DT."""
from pathlib import Path
import argparse,hashlib,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from audit_boot_tree import fdt
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'reports/cpu-hotplug-stage100-dt.json');a=p.parse_args()
def cells(raw):return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
errors=[];checks=[]
def need(ok,msg):
 checks.append({'check':msg,'passed':bool(ok)})
 if not ok:errors.append(msg)
raw=(ROOT/'device-baseline/running.dtb').read_bytes();tree,_=fdt(raw)
ph={cells(v['phandle'])[0]:n for n,v in tree.items() if 'phandle' in v}
nodes=[n for n,v in tree.items() if 'qcom,cpu-hotplug' in strings(v.get('compatible',b''))]
need(len(nodes)==1,'one qcom,cpu-hotplug node')
cpu_nodes=sorted([n for n,v in tree.items() if strings(v.get('device_type',b''))==['cpu']],key=lambda n:cells(tree[n]['reg'])[-1])
index={n:i for i,n in enumerate(cpu_nodes)}
kids=[n for n in sorted(tree) if n.startswith(nodes[0]+'/') and n.count('/')==nodes[0].count('/')+1]
rows=[]
for k in kids:
 v=tree[k];cpu=index.get(ph.get(cells(v.get('qcom,cpu',b'\0\0\0\0'))[0]))
 alias=strings(v['qcom,cdev-alias'])[0] if 'qcom,cdev-alias' in v else None
 refs=[n for n,m in tree.items() if 'cooling-device' in m and cells(m['cooling-device'])[0]==cells(v['phandle'])[0]] if 'phandle' in v else []
 rows.append({'node':k,'cpu':cpu,'alias':alias,'cooling_cells':cells(v.get('#cooling-cells',b'')),'map_refs':len(refs),'zones':sorted({r.split('/cooling-maps/')[0].rsplit('/',1)[1] for r in refs})})
need([r['cpu'] for r in rows]==list(range(8)),'8 children map to CPUs 0-7 in order')
need(all(r['alias']==f"cpu-hotplug{r['cpu']}" for r in rows),'aliases cpu-hotplugN match CPU index')
need(all(r['cooling_cells']==[2] for r in rows),'#cooling-cells 2 on every child (OF cdev)')
need(sum(r['map_refs'] for r in rows)==20,'20 saved cooling-map references bind these OF cdevs')
snap={l.split('\t')[1]:int(l.split('\t')[2]) for l in (ROOT/'reports/thermal-stage98-phone-cdevs.tsv').read_text().splitlines() if l}
need(all(snap.get(f'cpu-hotplug{i}')==1 for i in range(8)),'phone 6.12 cpu-hotplug0-7 present with max_state 1')
report={'scope':'saved 6.12 DT + read-only phone sysfs; no phone DT edit','dtb_sha256':hashlib.sha256(raw).hexdigest(),'children':rows,'checks':checks,'passed':not errors,'errors':errors}
a.output.write_text(json.dumps(report,indent=1)+'\n');print(json.dumps({'passed':not errors,'errors':errors},indent=1))
raise SystemExit(bool(errors))
