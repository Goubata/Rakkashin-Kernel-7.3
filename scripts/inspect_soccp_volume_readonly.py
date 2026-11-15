#!/usr/bin/env python3
"""Inspect only the two SoCCP volume partitions in memory, without mounting or writing."""
from pathlib import Path
import hashlib
import json
import struct

def inspect(path):
 with path.open('rb') as f: data=f.read(16*1024*1024+1)
 if len(data)>16*1024*1024:raise ValueError('volume exceeds bounded reader')
 u16=lambda off:struct.unpack_from('<H',data,off)[0]
 u32=lambda off:struct.unpack_from('<I',data,off)[0]
 bps=u16(11);spc=data[13];reserved=u16(14);fats=data[16];roots=u16(17)
 total=u16(19) or u32(32);fat_sectors=u16(22) or u32(36)
 if bps not in [512,1024,2048,4096] or not spc or spc&(spc-1) or not reserved or not fats or not fat_sectors or data[510:512]!=b'\x55\xaa':raise ValueError('invalid FAT BPB')
 root_sectors=(roots*32+bps-1)//bps;first=reserved+fats*fat_sectors+root_sectors
 if total*bps>len(data) or first>=total:raise ValueError('volume bounds')
 count=(total-first)//spc;bits=12 if count<4085 else 16 if count<65525 else 32
 cluster_bytes=bps*spc;fat=data[reserved*bps:(reserved+fat_sectors)*bps]
 def chain(cluster):
  seen=set();out=bytearray()
  while cluster>=2:
   if cluster in seen or cluster>=count+2:raise ValueError('bad/cyclic cluster chain')
   seen.add(cluster);off=(first+(cluster-2)*spc)*bps
   if off+cluster_bytes>total*bps:raise ValueError('cluster bounds')
   out+=data[off:off+cluster_bytes]
   if bits==12:
    at=cluster+cluster//2
    if at+2>len(fat):raise ValueError('FAT12 bounds')
    value=int.from_bytes(fat[at:at+2],'little');value=(value>>4) if cluster&1 else value&4095
   else:
    at=cluster*(bits//8)
    if at+bits//8>len(fat):raise ValueError('FAT entry bounds')
    value=int.from_bytes(fat[at:at+bits//8],'little')& (65535 if bits==16 else 0xfffffff)
   if value >= {12:0xff8,16:0xfff8,32:0xffffff8}[bits]:break
   if value<2:raise ValueError('unterminated allocated chain')
   cluster=value
  return bytes(out)
 files=[];visited=set();errors=[]
 def directory(raw,prefix='',depth=0):
  if depth>8:raise ValueError('directory nesting limit')
  long_names={}
  for off in range(0,len(raw)-31,32):
   entry=raw[off:off+32]
   if entry[0]==0:break
   if entry[0]==0xe5:long_names={};continue
   if entry[11]==15:
    seq=entry[0]&31
    if entry[0]&64:long_names={}
    long_names[seq]=entry[1:11]+entry[14:26]+entry[28:32];continue
   base=entry[:8].decode('ascii',errors='replace').rstrip();ext=entry[8:11].decode('ascii',errors='replace').rstrip()
   name=base+('.'+ext if ext else '')
   if long_names:
    units=b''.join(long_names[k] for k in sorted(long_names));chars=[units[i:i+2] for i in range(0,len(units),2)];valid=[]
    for unit in chars:
     if unit in [b'\0\0',b'\xff\xff']:break
     valid.append(unit)
    name=b''.join(valid).decode('utf-16-le',errors='replace');long_names={}
   if entry[11]&8 or name in ['.','..']:continue
   cluster=struct.unpack_from('<H',entry,26)[0] | (struct.unpack_from('<H',entry,20)[0]<<16 if bits==32 else 0)
   size=struct.unpack_from('<I',entry,28)[0];full=prefix+'/'+name
   if entry[11]&16:
    if cluster in visited:raise ValueError('directory cycle')
    visited.add(cluster);directory(chain(cluster),full,depth+1);continue
   if len(files)>=1024:raise ValueError('file count limit')
   payload=chain(cluster) if size else b''
   if size>len(payload):raise ValueError('file exceeds cluster chain')
   payload=payload[:size];row={'path':full,'size':size,'sha256':hashlib.sha256(payload).hexdigest()}
   if payload[:4]==b'\x7fELF' and len(payload)>=20:row.update(format='ELF',elf_class={1:32,2:64}.get(payload[4]),machine=int.from_bytes(payload[18:20],'little'))
   elif payload[:2]==b'MZ':row['format']='MZ/PE candidate'
   else:row['format']='other'
   files.append(row)
 root=chain(u32(44)) if bits==32 else data[(reserved+fats*fat_sectors)*bps:(reserved+fats*fat_sectors)*bps+roots*32]
 directory(root)
 return {'path':str(path),'size':len(data),'volume_sha256':hashlib.sha256(data).hexdigest(),'fat_bits':bits,'sector_bytes':bps,'cluster_bytes':cluster_bytes,'files':files}
report={'scope':'read-only bounded FAT inventory and file digests/ELF identities; no extraction, mounting, modification or firmware execution','partition_written':False,'images':[]}
for name in ['/dev/block/by-name/soccp_a','/dev/block/by-name/soccp_b']:
 try:report['images'].append(inspect(Path(name)))
 except (OSError,ValueError,struct.error) as error:report['images'].append({'path':name,'error':str(error)})
print(json.dumps(report,indent=2))
