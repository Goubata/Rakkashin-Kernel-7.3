#!/usr/bin/env python3
"""Verify exact pinned RTC source and locally owned external Makefile."""
import hashlib
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
r=json.loads((ROOT/'reports/rtc-source-lock.json').read_text())
assert r['source_commit']=='4dd306a07b2f456fd70ddaa28f4f65640374f202'
raw=subprocess.check_output(['git','-C',str(ROOT.parent/'avr-upstream-audit/android-7.3'),'show',r['source_commit']+':'+r['source']])
assert hashlib.sha256(raw).hexdigest()==r['sha256']
assert (ROOT/r['file']).read_bytes()==raw,'RTC source changed: review and record changes first'
assert (ROOT/'ports/qcom-rtc/Makefile').read_text()=='# SPDX-License-Identifier: GPL-2.0-only\nobj-m += rtc-pm8xxx.o\n'
print('Verified unmodified pinned native RTC source and external Makefile')
