#!/usr/bin/env python3
"""Read stock input identity and wake settings without injecting key events."""
import json
from pathlib import Path
rows={}
for node in Path('/sys/class/input').glob('input*'):
 try:
  name=(node/'name').read_text().strip()
  if name in ['pmic_pwrkey','pmic_resin']:rows[name]=(node/'device/power/wakeup').read_text().strip()
 except OSError:pass
print(json.dumps({'scope':'running 6.12 input metadata read only; no key injection or input device writes','wakeup':rows,'proc_input_devices':Path('/proc/bus/input/devices').read_text(),'writes':False},indent=2))
