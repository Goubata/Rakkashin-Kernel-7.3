#!/usr/bin/env python3
"""Check saved cpu-cluster cooling DT against phone cpufreq tables/cdevs and the fixture; never write DT."""
from pathlib import Path
import argparse,hashlib,json,re,struct,sys
ROOT=Path(__file__).resolve().parents[1]
sys.path.insert(0,str(ROOT/'scripts'))
from audit_boot_tree import fdt
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--output',type=Path,default=ROOT/'reports/cpufreq-cooling-stage99-dt.json');a=p.parse_args()
def cells(raw):return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
errors=[];checks=[]
def need(ok,msg):
 checks.append({'check':msg,'passed':bool(ok)})
 if not ok:errors.append(msg)
raw=(ROOT/'device-baseline/running.dtb').read_bytes();tree,_=fdt(raw)
ph={cells(v['phandle'])[0]:n for n,v in tree.items() if 'phandle' in v}
nodes=[n for n,v in tree.items() if 'qcom,cpufreq-cdev' in strings(v.get('compatible',b''))]
need(len(nodes)==1,'one qcom,cpufreq-cdev node')
clusters={}
for n in sorted(tree):
 if n.startswith(nodes[0]+'/') and n.count('/')==nodes[0].count('/')+1:
  cpus=[ph.get(x) for x in cells(tree[n].get('qcom,cpus',b''))]
  regs=[cells(tree[c]['reg'])[-1] for c in cpus if c]
  clusters[n.rsplit('/',1)[1]]={'cpus':cpus,'mpidr':regs,'first_cpu_index':None}
need(sorted(clusters)==['cpu-cluster0','cpu-cluster1'],'two clusters cpu-cluster0/1')
cpu_nodes=sorted([n for n,v in tree.items() if strings(v.get('device_type',b''))==['cpu']],key=lambda n:cells(tree[n]['reg'])[-1])
index={n:i for i,n in enumerate(cpu_nodes)}
for name,c in clusters.items():
 need(all(x in index for x in c['cpus']),name+': every qcom,cpus phandle is a CPU node')
 c['logical']=[index[x] for x in c['cpus'] if x in index];c['first_cpu_index']=c['logical'][0] if c['logical'] else None
need(clusters['cpu-cluster0']['logical']==[0,1,2,3,4,5],'cluster0 = CPUs 0-5')
need(clusters['cpu-cluster1']['logical']==[6,7],'cluster1 = CPUs 6-7')
need(all(len(n)<20 for n in clusters),'type names fit THERMAL_NAME_LENGTH')
tables={};boost={}
for line in (ROOT/'reports/cpufreq-stage99-phone-tables.txt').read_text().splitlines():
 f=line.split()
 if len(f)>2 and f[1]!='boost:':tables[f[0]]=[int(x) for x in f[2:]]
 elif len(f)>1 and f[1]=='boost:':boost[f[0]]=[int(x) for x in f[2:] if x.isdigit()]
snap={l.split('\t')[1]:int(l.split('\t')[2]) for l in (ROOT/'reports/thermal-stage98-phone-cdevs.tsv').read_text().splitlines() if l}
need(snap.get('cpu-cluster0')==len(tables['policy0'])+len(boost.get('policy0',[]))-1,'phone cpu-cluster0 max_state == policy0 valid entries-1 (23)')
need(snap.get('cpu-cluster1')==len(tables['policy6'])+len(boost.get('policy6',[]))-1,'phone cpu-cluster1 max_state == policy6 entries incl. 4 boost -1 (24)')
t=(ROOT/'tests/cpufreq-cooling/avr-cpufreq-cooling-test.c').read_text()
def arr(name):return [int(x) for x in re.search(name+r'\[\] = \{([^}]*)\}',t).group(1).replace('\n',' ').split(',') if x.strip()]
need(arr('phone_policy0')==tables['policy0'],'fixture policy0 table equals phone')
need(arr('phone_policy6')==tables['policy6'] and arr('phone_policy6_boost')==boost['policy6'],'fixture policy6 table+boost equals phone')
maps=[n for n,v in tree.items() if 'cooling-device' in v and nodes and ph.get(cells(v['cooling-device'])[0],'').startswith(nodes[0])]
need(not maps,'no kernel DT cooling-map references cpu-cluster cdevs (userspace HAL by type)')
report={'scope':'saved 6.12 DT + read-only phone sysfs; no phone DT edit','dtb_sha256':hashlib.sha256(raw).hexdigest(),'clusters':clusters,'phone_tables':tables,'phone_boost':boost,'checks':checks,'passed':not errors,'errors':errors}
a.output.write_text(json.dumps(report,indent=1)+'\n');print(json.dumps({'passed':not errors,'errors':errors},indent=1))
raise SystemExit(bool(errors))
