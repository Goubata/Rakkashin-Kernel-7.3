#!/usr/bin/env python3
"""Pin private capability fixture to saved read-only 6.12 sysfs ceilings."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]
p=ROOT/'reports/qmi-cooling-stage95-phone-capabilities.json';data=p.read_bytes();r=json.loads(data)
sha=hashlib.sha256(data).hexdigest()
expected='/* Recorded running 6.12 sysfs ceilings only; not a firmware-list dump.\n * Input SHA256: '+sha+' */\nstatic const struct {const char *node;u8 cap;} phone_caps[]={\n'
for row in r['observed_cooling_devices']:
 assert 0<=row['max_state']<=255
 expected+=' {"'+row['name']+'",'+str(row['max_state'])+'},\n'
expected+='};\n'
assert len(r['observed_cooling_devices'])==25
assert (ROOT/'tests/qmi-cooling/phone-caps.h').read_text()==expected
print('Verified 25 recorded sysfs ceilings; this is not an actual firmware capability-list dump')
