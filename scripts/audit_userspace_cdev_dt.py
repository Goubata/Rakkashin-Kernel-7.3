#!/usr/bin/env python3
"""Check saved userspace cooling devices (display-fps) and their maps vs phone; never write DT."""
from pathlib import Path
import hashlib,json,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from audit_boot_tree import fdt
def cells(raw):return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
errors=[];checks=[]
def need(ok,msg):
 checks.append({'check':msg,'passed':bool(ok)})
 if not ok:errors.append(msg)
raw=(ROOT/'device-baseline/running.dtb').read_bytes();tree,_=fdt(raw)
nodes=[n for n,v in tree.items() if 'qcom,userspace-cooling-devices' in strings(v.get('compatible',b''))]
need(len(nodes)==1,'one qcom,userspace-cooling-devices node')
kids=[n for n in sorted(tree) if n.startswith(nodes[0]+'/') and n.count('/')==nodes[0].count('/')+1]
rows=[]
for k in kids:
 v=tree[k];ph=cells(v['phandle'])[0] if 'phandle' in v else None
 maps=[(n,cells(m['cooling-device'])[1:3]) for n,m in tree.items() if 'cooling-device' in m and ph and cells(m['cooling-device'])[0]==ph]
 rows.append({'node':k,'max_level':cells(v['qcom,max-level'])[0],'maps':maps})
need([r['node'].rsplit('/',1)[1] for r in rows]==['display-fps'],'single child display-fps')
need(rows[0]['max_level']==16,'display-fps max level 16')
need(sorted(m[1] for m in rows[0]['maps'])==[[1,1],[2,2],[3,3]],'three saved maps select levels 1/2/3 (fixture mirrors them)')
snap={l.split('\t')[1]:int(l.split('\t')[2]) for l in (ROOT/'reports/thermal-stage98-phone-cdevs.tsv').read_text().splitlines() if l}
need(snap.get('display-fps')==16,'phone 6.12 display-fps max_state 16')
out=ROOT/'reports/userspace-cdev-stage101-dt.json'
out.write_text(json.dumps({'scope':'saved 6.12 DT + read-only phone sysfs; no phone DT edit','dtb_sha256':hashlib.sha256(raw).hexdigest(),'children':rows,'checks':checks,'passed':not errors,'errors':errors},indent=1)+'\n')
print(json.dumps({'passed':not errors,'errors':errors}));raise SystemExit(bool(errors))
