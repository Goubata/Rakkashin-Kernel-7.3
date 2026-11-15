#!/usr/bin/env python3
"""Selected factory panel notification layout/prototypes and stock client count."""
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import tarfile
ROOT=Path(__file__).resolve().parents[1]
PIN='037aa1c6c1a5f1e8017e7ce3c07ea870e8f344a1'
source='include/linux/soc/qcom/panel_event_notifier.h'
original=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/qcom-soc'),'show',PIN+':'+source])
staged=(ROOT/'ports/qcom-panel-event'/source).read_bytes()
def selected(raw):
    text=re.sub(r'/\*.*?\*/','',raw.decode(),flags=re.S)
    # This optional two-client extension is absent from AVR factory binary.
    text=re.sub(r'#ifdef CONFIG_MOT_QCOM_PANEL_NOTIFIER.*?#endif','',text,flags=re.S)
    enums={}
    for name,body in re.findall(r'enum (panel_event_\w+)\s*\{([^}]+)\}',text):
        enums[name]={value.strip():index for index,value in enumerate(body.split(',')) if value.strip()}
    payloads={name:' '.join(body.split()) for name,body in re.findall(r'struct (panel_event_notification\w*)\s*\{([^}]+)\}',text)}
    functions={}
    for name in ['panel_event_notifier_register','panel_event_notifier_unregister','panel_event_notification_trigger']:
        declaration=re.search(r'void\s*\*?\s*'+name+r'\([^;{]*\);',text).group(0)
        functions[name]=' '.join(declaration.split())
    return {'enums':enums,'payloads':payloads,'function_declarations':functions}
a,b=selected(original),selected(staged)
assert a==b and b['enums']['panel_event_notifier_client']['PANEL_EVENT_NOTIFIER_CLIENT_MAX']==5
with tarfile.open(ROOT/'device-baseline/vendor-modules.tar') as archive:
    member=next(x for x in archive if x.name.endswith('/panel_event_notifier.ko'))
    raw=archive.extractfile(member).read()
off=struct.unpack_from('<Q',raw,40)[0];ents,count=struct.unpack_from('<HH',raw,58)
sections=[struct.unpack_from('<IIQQQQIIQQ',raw,off+i*ents) for i in range(count)]
size=None
for section in sections:
    if section[1]!=2:continue
    strings=sections[section[6]];names=raw[strings[4]:strings[4]+strings[5]]
    for pos in range(section[4],section[4]+section[5],section[9]):
        index,_,_,_,_,symbol_size=struct.unpack_from('<IBBHQQ',raw,pos)
        if names[index:].split(b'\0',1)[0]==b'panel_event_notifier_entries':size=symbol_size
assert size==160 # pinned ARM64 entry: three pointers + enum + alignment = 32
r={'scope':'selected C declarations/payloads/enums and archived static array geometry; not full display/touch runtime ABI proof','passed':True,'factory_commit':PIN,'factory_header_sha256':hashlib.sha256(original).hexdigest(),'staged_header_sha256':hashlib.sha256(staged).hexdigest(),'selected':b,'stock_module':member.name,'stock_module_sha256':hashlib.sha256(raw).hexdigest(),'stock_client_array_bytes':size,'stock_client_count':5,'optional_mot_two_client_extension_enabled':False,'physical_verified':False,'pending':['Actual display producers and touch/charger consumers','Unregister must sleep outside notification callbacks; caller must unregister cookie once','All remaining source/ROM/physical acceptance gates']}
(ROOT/'reports/panel-event-stage52-abi.json').write_text(json.dumps(r,indent=2)+'\n')
print('Verified selected panel declarations/payloads/enums and five-client stock array')
