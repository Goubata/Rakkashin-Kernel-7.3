#!/usr/bin/env python3
"""Read only one SDAM size metadata byte per running stock NVMEM provider."""
from datetime import datetime,timezone
import json
import os
from pathlib import Path
rows=[]
for path in sorted(Path('/sys/bus/nvmem/devices').glob('spmi_sdam*')):
    row={'path':str(path),'resolved':str(path.resolve())}
    try:
        row['of_node']=str((path/'of_node').resolve())
        # A single SDAM_SIZE register byte; never data cells or PBS triggers.
        fd=os.open(path/'nvmem',os.O_RDONLY)
        try:raw=os.pread(fd,1,0x44)
        finally:os.close(fd)
        if len(raw)!=1:raise ValueError('size metadata read was not one byte')
        row.update(size_register=raw[0],data_bytes=raw[0]*32,data_window_start=0x40,data_window_end_exclusive=0x40+raw[0]*32)
    except (OSError,ValueError) as error:row['error']=str(error)
    rows.append(row)
print(json.dumps({'scope':'running stock 6.12 sysfs NVMEM metadata read only; no candidate probe or data/PBS access','captured_utc':datetime.now(timezone.utc).isoformat(),'provider_count':len(rows),'reads_per_provider':1,'read_offset':0x44,'read_length':1,'writes':False,'candidate_hardware_verified':False,'providers':rows},indent=2))
