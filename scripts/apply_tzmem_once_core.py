#!/usr/bin/env python3
"""Fix qcom_tzmem_enable() running SHM bridge setup inside DO_ONCE (android18-7.3).

DO_ONCE() calls its function with the global once_lock spinlock held and IRQs
off. In CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE, qcom_tzmem_init() issues SCM calls
(__qcom_scm_is_call_available + SHM_BRIDGE_ENABLE) that take qcom_scm_lock (a
mutex) and may sleep in TZ wait queues. Lockdep reports "Invalid wait context"
on the first qcom_scm probe (found with the Stage109 fake-TZ QEMU test); on the
phone it is a sleep-in-atomic. DO_ONCE_SLEEPABLE() keeps the run-once semantics
with a mutex.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
p = SRC / 'drivers/firmware/qcom/qcom_tzmem.c'
s = p.read_text()
old = '	DO_ONCE(qcom_tzmem_do_init, &result);\n'
new = '	/* qcom_tzmem_init() issues sleeping SCM calls: not under once_lock */\n	DO_ONCE_SLEEPABLE(qcom_tzmem_do_init, &result);\n'
if new not in s:
    if s.count(old) != 1:
        raise SystemExit('anchor missing')
    p.write_text(s.replace(old, new, 1))
print('applied tzmem DO_ONCE_SLEEPABLE fix to', SRC)
