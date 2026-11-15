#!/usr/bin/env python3
"""Read cached stock input/SPI/relay identities only, without device IO or writes."""
from pathlib import Path
import json
from datetime import datetime,timezone
report={'scope':'running stock driver/input metadata only; no SPI transfers, firmware commands, key injection or device writes','captured_utc':datetime.now(timezone.utc).isoformat(),'writes':False,'candidate_verified':False}
driver=Path('/sys/bus/platform/drivers/mmi_relay')
report['relay_driver_present']=driver.is_dir()
report['relay_bound_devices']=[p.name for p in driver.iterdir() if p.is_symlink() and p.name not in ['module','subsystem']] if driver.is_dir() else []
report['inputs']=[]
for node in sorted(Path('/sys/class/input').glob('input*')):
    row={'path':str(node)}
    for key in ['name','phys']:
        try:row[key]=(node/key).read_text().strip()
        except OSError as error:row[key]={'error':str(error)}
    report['inputs'].append(row)
report['spi_devices']=[]
for node in sorted(Path('/sys/bus/spi/devices').glob('*')):
    row={'path':str(node),'driver':str((node/'driver').resolve())}
    try:row['modalias']=(node/'modalias').read_text().strip()
    except OSError as error:row['error']=str(error)
    report['spi_devices'].append(row)
print(json.dumps(report,indent=2))
