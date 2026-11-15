#!/usr/bin/env python3
"""Read only SoCCP sysfs/partition ELF headers; never writes block devices or boots firmware."""
from pathlib import Path
import fcntl
import hashlib
import json
import os
import struct
from datetime import datetime, timezone
report={'scope':'read-only live remoteproc identity and SoCCP firmware ELF headers only','partition_written':False,'firmware_booted':False,'captured_utc':datetime.now(timezone.utc).isoformat(),'remoteprocs':[],'images':[],'firmware_runtime':{}}
for name in ['/proc/sys/kernel/firmware_config/force_sysfs_fallback','/proc/sys/kernel/firmware_config/ignore_sysfs_fallback','/sys/module/firmware_class/parameters/path']:
 try:report['firmware_runtime'][name]=Path(name).read_text().strip()
 except OSError as error:report['firmware_runtime'][name]={'error':str(error)}
report['rom_config_evidence']={}
for name in ['/vendor/etc/ueventd.rc','/vendor/etc/init/hw/init.qti.kernel.rc']:
 try:
  raw=Path(name).read_bytes()
  report['rom_config_evidence'][name]={'sha256':hashlib.sha256(raw).hexdigest(),'selected_lines':[line for line in raw.decode().splitlines() if 'firmware_directories' in line or 'force_sysfs_fallback' in line]}
 except OSError as error:report['rom_config_evidence'][name]={'error':str(error)}
for node in sorted(Path('/sys/class/remoteproc').glob('remoteproc*')):
 row={'path':str(node)}
 for name in ['name','firmware','state']:
  try:row[name]=(node/name).read_text().strip()
  except OSError as error:row[name]={'error':str(error)}
 report['remoteprocs'].append(row)
candidates=[]
for base in ['/dev/block/by-name','/dev/block/bootdevice/by-name','/vendor/firmware_mnt/image','/vendor/firmware','/vendor/soccp_firmware/image']:
 candidates += sorted(Path(base).glob('soccp*'))
seen=set()
for path in candidates:
 if path.is_dir():continue
 resolved=str(path.resolve())
 if resolved in seen:continue
 seen.add(resolved)
 row={'path':str(path),'resolved':resolved}
 try:
  with path.open('rb') as stream:
   raw=stream.read(64)
   row['prefix_hex']=raw[:16].hex();row['header_sha256']=hashlib.sha256(raw).hexdigest();row['elf_magic']=raw[:4]==b'\x7fELF'
   try:row['size']=struct.unpack('Q',fcntl.ioctl(stream.fileno(),0x80081272,bytes(8)))[0]
   except OSError:row['size']=os.fstat(stream.fileno()).st_size
   if row['elf_magic'] and raw[5]==1 and raw[4] in [1,2]:
    cls=raw[4];fmt='<HHIIIIIHHHHHH' if cls==1 else '<HHIQQQIHHHHHH';h=struct.unpack_from(fmt,raw,16)
    row.update(elf_class=32 if cls==1 else 64,machine=h[1],entry=h[3],program_header_offset=h[4],program_header_size=h[8],program_header_count=h[9],elf_header_size=h[7],section_header_offset=h[5],section_header_size=h[10],section_header_count=h[11])
    expected=32 if cls==1 else 56;off,size,count=h[4],h[8],h[9]
    if size!=expected or count>1024 or off>row['size'] or count*size>row['size']-off:raise ValueError('invalid/bounded ELF program header geometry')
    stream.seek(off);table=stream.read(size*count)
    if len(table)!=size*count:raise ValueError('truncated program headers')
    row['program_header_sha256']=hashlib.sha256(table).hexdigest();row['program_headers']=[]
    for i in range(count):
     values=struct.unpack_from('<IIIIIIII' if cls==1 else '<IIQQQQQQ',table,i*size)
     if cls==1:typ,foff,vaddr,paddr,filesz,memsz,flags,align=values
     else:typ,flags,foff,vaddr,paddr,filesz,memsz,align=values
     row['program_headers'].append(dict(index=i,type=typ,flags=flags,offset=foff,paddr=paddr,filesz=filesz,memsz=memsz))
   if row['size'] <= 16*1024*1024:
    stream.seek(0);row['content_sha256']=hashlib.sha256(stream.read(row['size'])).hexdigest()
   if not row['elf_magic']:
    stream.seek(0);prefix=stream.read(min(row['size'],1024*1024))
    row['prefix_scan_bytes']=len(prefix);row['prefix_scan_sha256']=hashlib.sha256(prefix).hexdigest()
    row['embedded_elf_ident_offsets']=[]
    at=prefix.find(b'\x7fELF')
    while at>=0 and len(row['embedded_elf_ident_offsets'])<64:
     row['embedded_elf_ident_offsets'].append({'offset':at,'class_byte':prefix[at+4] if at+4<len(prefix) else None})
     at=prefix.find(b'\x7fELF',at+1)
 except (OSError,ValueError,struct.error) as error:row['error']=str(error)
 report['images'].append(row)
print(json.dumps(report,indent=2))
