#!/usr/bin/env python3
"""Audit saved boot inputs. Extract only into ignored artifacts; never touch partitions."""
from pathlib import Path
import hashlib
import json
import re
import struct

ROOT=Path(__file__).resolve().parents[1]
BASE=ROOT/'device-baseline'
OUT=ROOT/'artifacts/dt-input-audit'

def align(value,page):
    if page<=0 or page & (page-1):raise ValueError('invalid page alignment')
    return (value+page-1)&-page

def region(data,offset,size):
    if offset<0 or size<0 or offset+size>len(data):raise ValueError('region outside input')
    return data[offset:offset+size]

def cstr(data,offset,limit):
    end=data.find(b'\0',offset,limit)
    if offset<0 or end<offset:raise ValueError('unterminated string')
    return data[offset:end].decode('utf-8','strict'),end+1

def fdt(data):
    if len(data)<40:raise ValueError('short FDT')
    magic,total,structoff,stringoff,rsv,version,last,cpu,stringsize,structsize=struct.unpack_from('>10I',data)
    if magic!=0xd00dfeed or total>len(data) or total<40 or version<17:raise ValueError('unsupported FDT header')
    body=region(data,structoff,structsize);strings=region(data,stringoff,stringsize)
    if structoff+structsize>total or stringoff+stringsize>total:raise ValueError('FDT bounds')
    nodes={};stack=[];offset=0;ended=False
    while offset+4<=len(body):
        token=struct.unpack_from('>I',body,offset)[0];offset+=4
        if token==1:
            name,offset=cstr(body,offset,len(body));offset=align(offset,4)
            if not stack and name:raise ValueError('nonempty root name')
            stack.append(name);path='/'+'/'.join(stack[1:])
            if path in nodes:raise ValueError('duplicate FDT node')
            nodes[path]={}
        elif token==2:
            if not stack:raise ValueError('unbalanced FDT nodes')
            stack.pop()
        elif token==3:
            if not stack:raise ValueError('property outside node')
            size,nameoff=struct.unpack('>II',region(body,offset,8));offset+=8
            name,_=cstr(strings,nameoff,len(strings));value=region(body,offset,size);offset=align(offset+size,4)
            path='/'+'/'.join(stack[1:])
            if name in nodes[path]:raise ValueError('duplicate FDT property')
            nodes[path][name]=value
        elif token==4:continue
        elif token==9:
            if stack:raise ValueError('FDT ended with open nodes')
            ended=True;break
        else:raise ValueError(f'invalid FDT token {token}')
    if not ended:raise ValueError('missing FDT end')
    return nodes,total

def summary(nodes):
    root=nodes['/'];info={'node_count':len(nodes),'compatible_node_count':sum('compatible' in p for p in nodes.values()),
        'reserved_memory_nodes':sum(p.startswith('/reserved-memory/') for p in nodes)}
    for key in ['model','compatible']:
        if key in root:info[key]=[s.decode('utf-8','replace') for s in root[key].rstrip(b'\0').split(b'\0')]
    for key in ['qcom,msm-id','qcom,board-id','qcom,pmic-id']:
        if key in root and len(root[key])%4==0:info[key]=list(struct.unpack('>'+'I'*(len(root[key])//4),root[key]))
    return info

def compare(left,right):
    lp,rp=set(left),set(right)
    changes={}
    for path in sorted(lp&rp):
        if path=='/chosen' or path.startswith('/chosen/'):continue
        keys=set(left[path])|set(right[path])
        names=[k for k in sorted(keys) if left[path].get(k)!=right[path].get(k)]
        if names:changes[path]=names
    return {'left_only_nodes':sorted(lp-rp),'right_only_nodes':sorted(rp-lp),
            'changed_node_count_excluding_chosen':len(changes),'changed_property_names':changes}

def semantic_compare(left,right):
    def handles(nodes):
        result={}
        for path,props in nodes.items():
            raw=props.get('phandle',props.get('linux,phandle',b''))
            if len(raw)==4:
                number=int.from_bytes(raw,'big')
                if number in result:raise ValueError('duplicate phandle')
                result[number]=path
        return result
    lm,rm=handles(left),handles(right)
    raw=compare(left,right);renumbered=[];references=[];semantic={}
    def resolve(raw,mapping):
        if raw is None or len(raw)%4:return None
        values=struct.unpack('>'+'I'*(len(raw)//4),raw)
        if any(v not in mapping for v in values):return None
        return [mapping[v] for v in values]
    for path,names in raw['changed_property_names'].items():
        if path=='/__symbols__':continue
        changed=[]
        for name in names:
            if name in ('phandle','linux,phandle'):
                renumbered.append({'node':path,'property':name});continue
            if re.fullmatch(r'pinctrl-\d+',name) or name=='qcom,wsa-macro-handle':
                lv=resolve(left[path].get(name),lm);rv=resolve(right[path].get(name),rm)
                if lv is not None and lv==rv:
                    references.append({'node':path,'property':name,'targets':lv});continue
            changed.append(name)
        if changed:semantic[path]=changed
    return {'left_only_nodes':raw['left_only_nodes'],'right_only_nodes':raw['right_only_nodes'],
            'renumbered_phandles':renumbered,'equivalent_reference_properties':references,
            'changed_properties_excluding_chosen_and_symbol_metadata':semantic}

def public_report(value):
    if isinstance(value,str):
        return re.sub(r'(/hypervisor/qcom,resource-manager-rpc@)[0-9a-fA-F]+',r'\1<runtime-capability>',value)
    if isinstance(value,list):return [public_report(v) for v in value]
    if isinstance(value,dict):return {public_report(k):public_report(v) for k,v in value.items()}
    return value

def sha(path):
    h=hashlib.sha256()
    with path.open('rb') as f:
        while chunk:=f.read(1024*1024):h.update(chunk)
    return h.hexdigest()

def main():
    OUT.mkdir(parents=True,exist_ok=True)
    vendorpath=BASE/'images/vendor_boot_a.img';dtbopath=BASE/'images/dtbo_a.img'
    vendor=vendorpath.read_bytes()
    if vendor[:8]!=b'VNDRBOOT':raise ValueError('not vendor_boot')
    version,page=struct.unpack_from('<II',vendor,8)
    if version!=4:raise ValueError('expected vendor_boot v4')
    ramdisk_size=struct.unpack_from('<I',vendor,24)[0]
    header_size,dtb_size=struct.unpack_from('<II',vendor,2096)
    tablesize,entrycount,entrysize,bootcfgsize=struct.unpack_from('<4I',vendor,2112)
    if header_size<2128 or header_size>page or entrysize<108 or entrycount*entrysize>tablesize:raise ValueError('vendor table bounds')
    ramdiskoff=align(header_size,page);dtboff=ramdiskoff+align(ramdisk_size,page)
    tableoff=dtboff+align(dtb_size,page);table=region(vendor,tableoff,tablesize)
    region(vendor,tableoff+align(tablesize,page),bootcfgsize)
    fragments=[]
    for i in range(entrycount):
        ent=region(table,i*entrysize,entrysize);size,off,kind=struct.unpack_from('<3I',ent)
        if off+size>ramdisk_size:raise ValueError('ramdisk fragment outside area')
        name=ent[12:44].split(b'\0',1)[0].decode('ascii','strict')
        fragments.append({'index':i,'size':size,'offset':off,'type':kind,'name':name,
                          'board_id':list(struct.unpack_from('<16I',ent,44))})
    packed=region(vendor,dtboff,dtb_size);bases=[];basetrees=[];pos=0
    while pos<len(packed):
        if not any(packed[pos:]):break
        nodes,size=fdt(packed[pos:]);name=f'vendor-base-{len(bases)}.dtb'
        (OUT/name).write_bytes(packed[pos:pos+size]);basetrees.append(nodes)
        bases.append({'file':name,'offset_in_dtb_area':pos,'size':size,**summary(nodes)})
        pos+=size
        while pos<len(packed) and packed[pos]==0:pos+=1
    dtbo=dtbopath.read_bytes();header=struct.unpack_from('>8I',dtbo)
    magic,total,hsize,esize,count,eoff,dpage,dversion=header
    if magic!=0xd7b7ab1e or total>len(dtbo) or hsize<32 or esize<32 or dversion!=0 or eoff<hsize or eoff+count*esize>total:raise ValueError('DTBO table bounds')
    overlays=[]
    for i in range(count):
        size,offset,ident,rev,*custom=struct.unpack('>8I',region(dtbo,eoff+i*esize,32))
        blob=region(dtbo,offset,size)
        if offset+size>total:raise ValueError('DTBO payload outside total size')
        nodes,actual=fdt(blob);name=f'overlay-{i}.dtbo';(OUT/name).write_bytes(blob[:actual])
        overlays.append({'index':i,'file':name,'size':size,'payload_offset':offset,'fdt_size':actual,'id':ident,'revision':rev,'custom':custom,**summary(nodes)})
    livepath=BASE/'running.dtb';live,_=fdt(livepath.read_bytes())
    # Values such as runtime bootargs, serials and seeds are deliberately not
    # written to the public report. Full raw/decompiled trees stay ignored.
    report={'scope':'saved-input structure and names only; not 7.3 hardware validation or a flashable image',
       'input_sha256':{p.name:sha(p) for p in [vendorpath,dtbopath,livepath]},
       'vendor_boot':{'version':version,'page_size':page,'ramdisk_size':ramdisk_size,'dtb_size':dtb_size,
                      'dtb_offset':dtboff,'table_size':tablesize,'table_offset':tableoff,
                      'bootconfig_size':bootcfgsize,'fragments':fragments},
       'base_trees':bases,'dtbo':{'version':dversion,'total_size':total,'page_size':dpage,'entries':overlays},
       'running_tree':summary(live),
       'base_vs_running':[{ 'base':i,**compare(tree,live)} for i,tree in enumerate(basetrees)]}
    (ROOT/'reports/boot-tree-input-audit.json').write_text(json.dumps(public_report(report),indent=2)+'\n')
    print(json.dumps({'vendor_ramdisk_fragments':len(fragments),'base_dtbs':len(bases),'dtbo_entries':count,
                      'running':summary(live),'artifacts':str(OUT)},indent=2))
if __name__=='__main__':main()
