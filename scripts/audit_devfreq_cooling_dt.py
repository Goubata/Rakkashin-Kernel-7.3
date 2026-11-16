#!/usr/bin/env python3
"""Check saved GPU devfreq cooling DT/OPP semantics and the phone cdev snapshot; never write DT."""
from pathlib import Path
import argparse,hashlib,json,re,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from audit_boot_tree import fdt
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'reports/devfreq-cooling-stage98-dt.json');a=p.parse_args()
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned DT cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
errors=[];checks=[]
def need(ok,msg):
 checks.append({'check':msg,'passed':bool(ok)})
 if not ok:errors.append(msg)
raw=(ROOT/'device-baseline/running.dtb').read_bytes();tree,_=fdt(raw)
ph={cells(v['phandle'])[0]:n for n,v in tree.items() if 'phandle' in v}
def active(n):
 while n:
  if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'ok',b'okay'):return False
  n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
 return True
nodes=[n for n,v in tree.items() if 'qcom,devfreq-cdev' in strings(v.get('compatible',b'')) and active(n)]
need(len(nodes)==1,'one active qcom,devfreq-cdev node')
gpu=ph.get(cells(tree[nodes[0]].get('qcom,devfreq',b'\0\0\0\0'))[0])
need(gpu is not None and 'qcom,kgsl-3d0' in strings(tree[gpu].get('compatible',b'')) and active(gpu),'qcom,devfreq targets the active KGSL node')
levels=sorted([n for n in tree if n.startswith(gpu+'/qcom,gpu-pwrlevel-bins/') and '/qcom,gpu-pwrlevel@' in n],key=lambda n:int(n.rsplit('@',1)[1],16))
bins={n.split('/qcom,gpu-pwrlevel-bins/')[1].split('/')[0] for n in levels}
need(len(bins)==1,'single saved GPU power-level bin')
freqs=[cells(tree[n]['qcom,gpu-freq'])[0] for n in levels]
need(len(freqs)==21,'21 saved GPU power levels (user table)')
need(freqs==sorted(freqs,reverse=True) and len(set(freqs))==len(freqs),'levels strictly descending, unique')
test=(ROOT/'tests/devfreq-cooling/avr-devfreq-cooling-test.c').read_text()
fixture=[int(x) for x in re.search(r'saved_gpu_hz\[\] = \{([^}]*)\}',test).group(1).replace('\n',' ').split(',') if x.strip()]
need(fixture==freqs,'QEMU fixture OPP table equals saved DT table')
maps=[n for n,v in tree.items() if 'cooling-device' in v and cells(v['cooling-device'])[:1]==[cells(tree[gpu]['phandle'])[0]]]
need(cells(tree[gpu].get('#cooling-cells',b''))==[2] and maps,'GPU-node cooling maps bind KGSL own OF cdev (not this type-only cdev)')
snap=ROOT/'reports/thermal-stage98-phone-cdevs.tsv'
rows=[l.split('\t') for l in snap.read_text().splitlines() if l]
by={r[1]:int(r[2]) for r in rows}
need(by.get('gpu')==len(freqs)-1,'phone 6.12 cdev "gpu" max_state == levels-1')
need(by.get('kgsl')==len(freqs)-1,'phone 6.12 KGSL OF cdev max_state == levels-1')
need(by.get('ddr-cdev')==1,'phone ddr-cdev max_state 1 (Stage97 cross-check)')
report={'scope':'saved 6.12 DT + read-only phone sysfs snapshot; no phone DT edit','dtb_sha256':hashlib.sha256(raw).hexdigest(),'snapshot_sha256':hashlib.sha256(snap.read_bytes()).hexdigest(),'node':nodes[0],'gpu':gpu,'gpu_hz':freqs,'gpu_maps':maps,'checks':checks,'passed':not errors,'errors':errors}
a.output.write_text(json.dumps(report,indent=1)+'\n');print(json.dumps({'passed':not errors,'errors':errors,'maps':len(maps)},indent=1))
raise SystemExit(bool(errors))
