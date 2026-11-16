#!/usr/bin/env python3
"""Compare private native-probe DT to selected AVR name-candidate semantics."""
from pathlib import Path
import argparse,hashlib,json,struct
p=argparse.ArgumentParser(description=__doc__);p.add_argument("--output",type=Path,default=Path("reports/bcl-probe-stage91-dt.json"));args=p.parse_args()
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
files={'guest':'artifacts/qemu-bcl-pmic/virt-rail.dtb','candidate':'artifacts/dt-bcl-name-stage91/avr-bcl-name-candidate.dtb'}
raw={k:(ROOT/v).read_bytes() for k,v in files.items()};trees={k:fdt(v)[0] for k,v in raw.items()}
def cells(b):return list(struct.unpack('>'+str(len(b)//4)+'I',b))
def phmap(t):return {cells(p['phandle'])[0]:n for n,p in t.items() if 'phandle' in p}
ph={k:phmap(t) for k,t in trees.items()};checks=[]
def need(ok,msg):
 checks.append({'check':msg,'passed':bool(ok)})
 if not ok:raise ValueError(msg)
provider={'guest':'/avr-bcl-probe-bus/bcl@4700','candidate':'/soc/arbiter@c400000/spmi@c426000/qcom,pmih010x@7/bcl@4700'}
p={k:t[provider[k]] for k,t in trees.items()}
for key in ['reg','#thermal-sensor-cells','qcom,pmic7-threshold','nvmem-cell-names']:need(p['guest'][key]==p['candidate'][key],key+' provider contract matches')
for tag in trees:
 target=ph[tag][cells(p[tag]['nvmem-cells'])[0]];cell=trees[tag][target]
 need(cells(cell['reg'])==[0x5d,1] and cells(cell['bits'])==[5,1],tag+': SDAM bit5 byte0x5d')
mapping={'avr-probe-current0':'pmih010x-ibat-lvl0','avr-probe-current1':'pmih010x-ibat-lvl1','pmih010x-2s-ibat0':'pmih010x-2s-ibat0','pmih010x-2s-ibat1':'pmih010x-2s-ibat1','avr-probe-voltage':'vbat','avr-probe-level0':'pmih010x-bcl-lvl0','avr-probe-level1':'pmih010x-bcl-lvl1','avr-probe-level2':'pmih010x-bcl-lvl2'}
rows=[]
for guest,board in mapping.items():
 a='/thermal-zones/'+guest;b='/soc/thermal-zones/'+board;g=trees['guest'][a];v=trees['candidate'][b]
 gs=cells(g['thermal-sensors']);vs=cells(v['thermal-sensors'])
 need(len(gs)==len(vs)==2 and gs[1]==vs[1] and ph['guest'][gs[0]]==provider['guest'] and ph['candidate'][vs[0]]==provider['candidate'],board+': same sensor ID')
 for delay in ('polling-delay','polling-delay-passive'):need(g.get(delay,b'\0'*4)==v.get(delay,b'\0'*4),board+': '+delay)
 def trips(tag,base):return [{k:v for k,v in p.items() if k in ('temperature','hysteresis','type')} for n,p in trees[tag].items() if n.rsplit('/',1)[0]==base+'/trips']
 need(trips('guest',a)==trips('candidate',b),board+': trip metrics AND ordering match')
 need(len(guest)<20 and len(board)<20,board+': native thermal type fits')
 rows.append({'guest':a,'candidate':b,'sensor_id':gs[1],'trip_count':len(trips('guest',a))})
report={'passed':True,'scope':'Selected PMIH source IDs, NVMEM bit, ordered trips and polling scalar fixture only; guest IRQ chip/regmap are recorded, physical SPMI/cooling links not simulated','inputs':{k:{'file':files[k],'sha256':hashlib.sha256(v).hexdigest()} for k,v in raw.items()},'checks':checks,'zones':rows,'physical_verified':False,'whole_board_accepted':False}
(ROOT/args.output).write_text(json.dumps(report,indent=2)+'\n');print('Probe fixture matches eight declared PMIH zones, including ordered voltage trips')
