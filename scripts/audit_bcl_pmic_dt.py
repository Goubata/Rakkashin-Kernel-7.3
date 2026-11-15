#!/usr/bin/env python3
"""Audit pinned PMIC BCL source IDs against selected AVR DT, without hardware IO."""
from pathlib import Path
import argparse,hashlib,json,re,struct,subprocess
p=argparse.ArgumentParser(description=__doc__);p.add_argument("--port",action="store_true");p.add_argument("--output",type=Path,default=Path("reports/bcl-pmic-stage89-dt.json"));args=p.parse_args()
from audit_boot_tree import fdt
ROOT=Path(__file__).resolve().parents[1]
REPO=ROOT.parent/'avr-upstream-audit/qcom-soc'
PIN='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
def cells(raw):
 if len(raw)%4:raise ValueError('unaligned DT cells')
 return list(struct.unpack('>'+str(len(raw)//4)+'I',raw))
def strings(raw):return raw.rstrip(b'\0').decode().split('\0')
def sha(raw):return hashlib.sha256(raw).hexdigest()
source_paths=['drivers/thermal/qcom/'+n for n in ['qti_bcl_pmic5.c','qti_bcl_common.h','qti_bcl_stats.c']]
sources={p:subprocess.check_output(['git','-C',str(REPO),'show',PIN+':'+p]) for p in source_paths}
c_source=sources[source_paths[0]].decode();common=sources[source_paths[1]].decode()
enum=re.search(r'enum bcl_dev_type\s*\{(.*?)\};',common,re.S).group(1)
ids=[n.strip() for n in enum.split(',') if n.strip()];assert ids[-1]=='BCL_TYPE_MAX';ids=ids[:-1]
names=re.findall(r'"([^"]+)"',re.search(r'bcl_int_names\[BCL_TYPE_MAX\]\[25\]\s*=\s*\{(.*?)\};',c_source,re.S).group(1));assert len(ids)==len(names)==10
matches=dict(re.findall(r'\.compatible\s*=\s*"([^"]+)"\s*,\s*\.data\s*=\s*&([a-zA-Z_0-9]+)',c_source));assert matches['qcom,bcl-v5']=='pmih010x_data'
errors=[];inputs={}
for tag,name in [('saved','device-baseline/running.dtb'),('candidate','artifacts/dt-moto-relay-stage53/avr-moto-relay-candidate.dtb')]:
 raw=(ROOT/name).read_bytes();tree,_=fdt(raw);ph={cells(p['phandle'])[0]:n for n,p in tree.items() if 'phandle' in p};checks=[];selected={};providers={};zones={}
 def need(ok,msg):
  checks.append({'check':msg,'passed':bool(ok)})
  if not ok:errors.append(tag+': '+msg)
 def active(n):
  while n:
   if tree[n].get('status',b'okay\0').rstrip(b'\0') not in (b'ok',b'okay'):return False
   n=n.rsplit('/',1)[0] or ('/' if n!='/' else '')
  return True
 def raw_props(props,omit=()):return {k:v.hex() for k,v in props.items() if k not in ('phandle','linux,phandle',*omit)}
 def interrupt_parent(n):
  # Follow exact native of_irq_find_parent order: parent/explicit link first,
  # then stop on #interrupt-cells, including an ancestor controller with no
  # explicit interrupt-parent property of its own.
  seen=set()
  while n:
   if n in seen:raise ValueError('interrupt parent cycle')
   seen.add(n)
   n=ph[cells(tree[n]['interrupt-parent'])[0]] if 'interrupt-parent' in tree[n] else (n.rsplit('/',1)[0] or ('/' if n!='/' else ''))
   if n and '#interrupt-cells' in tree[n]:return n
  raise ValueError('missing interrupt parent')
 for n,p in tree.items():
  compat=[c for c in strings(p.get('compatible',b'')) if c in matches]
  if not compat:continue
  selected[n]=raw_props(p,('interrupts','nvmem-cells'))
  selected[n]['active']=active(n)
  selected[n]['source_match']=matches[compat[0]]
  if not active(n):
   selected[n]={**raw_props(p),'active':False,'source_match':matches[compat[0]]};continue
  providers[n]=p
  parent=n.rsplit('/',1)[0];reg=cells(p.get('reg',b''));preg=cells(tree[parent].get('reg',b''))
  need(cells(tree[parent]['#address-cells'])==[1] and cells(tree[parent]['#size-cells'])==[0],n+': factory one-address zero-size parent semantics')
  need(len(reg)==2 and reg[1]==0x100 and 0<=reg[0]<=0xffff-0xac,n+': saved two raw reg cells, first address fits native factory register reads')
  need(cells(p.get('#thermal-sensor-cells',b''))==[1],n+': one sensor ID argument')
  irqp=interrupt_parent(n);irq=cells(p.get('interrupts',b''));irqnames=strings(p.get('interrupt-names',b''))
  need(active(irqp) and cells(tree[irqp].get('#interrupt-cells',b''))==[4],n+': active four-cell interrupt controller')
  need(irqnames==names[5:8] and len(irq)==12,n+': three pinned level IRQ names/routes')
  if len(irq)==12 and reg and preg:
   need(all(irq[i:i+4]==[preg[0],reg[0]>>8,i//4,0] for i in range(0,12,4)),n+': SID/peripheral/level routes match parent and register base')
  selected[n]['interrupt_routes']={'controller':irqp,'cells':irq}
  refs=[];nv=cells(p.get('nvmem-cells',b''));nvnames=strings(p.get('nvmem-cell-names',b'')) if nv else []
  need(len(nv)==len(nvnames),n+': named NVMEM cells')
  for handle,cellname in zip(nv,nvnames):
   cell=ph.get(handle);need(cell is not None and active(cell),n+': active NVMEM configuration cell')
   if cell is None:continue
   cp=tree[cell];sdam=cell.rsplit('/',1)[0]
   need(cells(cp.get('reg',b''))==[0x5d,1] and cells(cp.get('bits',b''))==[5,1],n+': saved SMB1510 selection bit shape')
   need(cellname=='smb1510_present',n+': pinned standard/2S selection name')
   refs.append({'name':cellname,'node':cell,'cell':raw_props(cp),'provider':sdam,'provider_props':raw_props(tree[sdam])})
  selected[n]['nvmem']=refs
 need(len(providers)==2,'two active PMIC BCL providers; disabled duplicate kept unchanged')
 provider_handles={cells(p['phandle'])[0]:n for n,p in providers.items()}
 for n,p in tree.items():
  spec=cells(p.get('thermal-sensors',b''))
  if not spec or spec[0] not in provider_handles:continue
  need(active(n) and len(spec)==2 and 0<=spec[-1]<len(ids),n+': active single source-enumerated sensor')
  if len(spec)!=2 or spec[1]>=len(ids):continue
  provider=provider_handles[spec[0]];sensor=spec[1];zones[n]={'provider':provider,'id':sensor,'name':ids[sensor]}
  selected[n]={**raw_props(p,('thermal-sensors',)),'sensor':[provider,sensor,ids[sensor]]}
  for child,props in tree.items():
   if child.startswith(n+'/trips/'):
    temp=cells(props.get('temperature',b''));hyst=cells(props.get('hysteresis',b''))
    need(len(temp)==len(hyst)==1 and temp[0]<0x80000000 and hyst[0]<0x80000000,child+': single nonnegative factory metric/hysteresis')
    need(strings(props.get('type',b''))==['passive'],child+': passive factory trip')
    selected[child]=raw_props(props)
   if child.startswith(n+'/cooling-maps/'):
    trip=cells(props.get('trip',b''));t=ph.get(trip[0]) if len(trip)==1 else None
    need(t is not None and t.startswith(n+'/trips/'),child+': local trip phandle')
    spec2=cells(props.get('cooling-device',b''));links=[]
    while spec2:
     target=ph.get(spec2.pop(0));need(target is not None and active(target),child+': active cooling provider')
     if target is None:break
     arity=cells(tree[target].get('#cooling-cells',b''));need(arity==[2] and len(spec2)>=2,child+': two cooling bounds')
     if arity!=[2] or len(spec2)<2:break
     low,high=spec2[:2];del spec2[:2];need(low<=high,child+': ordered bounds')
     links.append([target,low,high])
    selected[child]={**raw_props(props,('trip','cooling-device')),'trip':t,'cooling':links}
 need(len(zones)==11,'eleven declared selected thermal zones, including alternative current pairs')
 for provider in providers:
  got={z['id'] for z in zones.values() if z['provider']==provider}
  expected={0,1,2,5,6,7,8,9} if '@7/' in provider else {5,6,7}
  need(got==expected,provider+': exact saved channel ID set')
 inputs[tag]={'file':name,'sha256':sha(raw),'providers':list(providers),'zones':zones,'selected':selected,'checks':checks}
preserved=inputs['saved']['selected']==inputs['candidate']['selected']
if not preserved:errors.append('Selected saved/candidate PMIC BCL semantics differ')
port_contract=None
if args.port:
 port=(ROOT/'ports/qcom-bcl-pmic/qti_bcl_pmic5.c').read_text();header=(ROOT/'ports/qcom-bcl-pmic/qti_bcl_common.h').read_text();checks=[]
 def check(ok,msg):
  checks.append({'check':msg,'passed':bool(ok)})
  if not ok:errors.append('port: '+msg)
 def compact(text):return re.sub(r'\s+','',text)
 check(compact(re.search(r'enum bcl_dev_type\s*\{(.*?)\};',header,re.S).group(1))==compact(enum),'sensor ID enum unchanged')
 check(re.findall(r'"([^"]+)"',re.search(r'bcl_int_names\[BCL_TYPE_MAX\]\[25\]\s*=\s*\{(.*?)\};',port,re.S).group(1))==names,'IRQ names and sensor indices unchanged')
 check(dict(re.findall(r'\.compatible\s*=\s*"([^"]+)"\s*,\s*\.data\s*=\s*&([a-zA-Z_0-9]+)',port))==matches,'compatible to calibration descriptor matches unchanged')
 for name in set(matches.values()):
  pattern=r'static const struct bcl_desc '+name+r'\s*=\s*\{(.*?)\n\};'
  check(compact(re.search(pattern,port,re.S).group(1))==compact(re.search(pattern,c_source,re.S).group(1)),name+': calibration registers and factors unchanged')
 def constants(text):return dict(re.findall(r'^#define\s+(BCL_[A-Z0-9_]+)\s+(.+)$',text,re.M))
 check(constants(port)==constants(c_source),'factory BCL numerical constants unchanged')
 port_contract={'source_sha256':sha(port.encode()),'header_sha256':sha(header.encode()),'checks':checks,'scope':'Selected ID, IRQ-name, match and calibration constants only; does not prove conversion/control/hardware behavior'}
r={'stage':90 if args.port else 89,'passed':not errors,'scope':'Pinned source IDs and selected DT interrupt, thermal, NVMEM and cooling-link shape only; driver not ported by this audit','port_contract':port_contract,'source_commit':PIN,'source_sha256':{p:sha(v) for p,v in sources.items()},'sensor_ids':dict(enumerate(ids)),'compatible_matches':matches,'inputs':inputs,'saved_semantics_preserved':preserved,'dt_modified':False,'driver_port_complete':False,'physical_verified':False,'whole_board_accepted':False,'observations':['Both active saved nodes use qcom,bcl-v5 and therefore select pmih010x_data in pinned factory source; no speculative PM8550 compatible change made','Parent has zero size cells: the factory of_get_address(...,0) consumes the first raw reg cell; the trailing 0x100 is not treated as a validated native resource size here','DT declares standard IDs0/1 and 2S IDs8/9 simultaneously; factory NVMEM chooses one current pair, so eleven declarations are not eleven proven active sensors','NVMEM reference denotes SMB1510 bit5 at SDAM offset0x5d; its physical value was not read or guessed','BCL event channels5/6/7 carry a logical asserted metric; VBAT/current channels use factory mV/mA scales rather than millidegree temperature'],'required_port_work':['Use real native thermal private layout/accessors, remove vendor change_mode helper dependency','Propagate NVMEM/IRQ/thermal/register-write errors instead of reporting successful partial probe','Own all per-device IRQ wake/enable, notifier, IPC-log and debugfs resources; fix probe/remove and freeze/restore unwind','Initialize notifier callback before registration and remove stale global allocation/count state','Retain physical calibration and validate conversions/endianness/threshold mapping with native regmap fault tests before hardware acceptance'],'errors':errors}
if args.port:
 r['port_acceptance_requirements']=r.pop('required_port_work')
 r['implementation_status']='Initial full-source 7.3 module draft exists; this audit checks selected DT/calibration contracts only'
(ROOT/args.output).write_text(json.dumps(r,indent=2)+'\n');print(json.dumps({'passed':r['passed'],'active_providers':len(inputs['candidate']['providers']),'declared_zones':len(inputs['candidate']['zones']),'errors':errors},indent=2));raise SystemExit(bool(errors))
