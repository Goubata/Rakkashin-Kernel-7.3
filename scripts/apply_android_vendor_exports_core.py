#!/usr/bin/env python3
"""Restore android16-6.12 ACK vendor-module interfaces that 7.3 dropped and that
phone modules import:
  - android_vh_check_uninterruptible_tasks / _dn (hung_task_enh.ko): called for
    every task in the hung-task scan; need_check=false skips the task (6.12)
  - EXPORT_SYMBOL_GPL(memblock_end_of_DRAM) (mem_offline.ko; arm64 keeps memblock)
  - EXPORT_SYMBOL_GPL(sched_clock_read_begin/_retry) (gh_ctrl.ko)
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old, new))


h = SRC / 'include/trace/hooks/hung_task.h'
if not h.exists():
    h.write_text('''/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM hung_task

#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_HUNG_TASK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_HUNG_TASK_H

#include <trace/hooks/vendor_hooks.h>

/* ANDROID: android16-6.12 parity */
DECLARE_HOOK(android_vh_check_uninterruptible_tasks,
	TP_PROTO(struct task_struct *t, unsigned long timeout,
		bool *need_check),
	TP_ARGS(t, timeout, need_check));

DECLARE_HOOK(android_vh_check_uninterruptible_tasks_dn,
	TP_PROTO(void *unused),
	TP_ARGS(unused));

#endif /* _TRACE_HOOK_HUNG_TASK_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
''')
V = 'drivers/android/vendor_hooks.c'
edit(V, '#include <trace/hooks/cgroup.h>\n', '#include <trace/hooks/cgroup.h>\n#include <trace/hooks/hung_task.h>\n')
s = (SRC / V).read_text()
if 'android_vh_check_uninterruptible_tasks)' not in s:
    s = s.rstrip('\n') + '\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_check_uninterruptible_tasks);\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_check_uninterruptible_tasks_dn);\n'
    (SRC / V).write_text(s)

H = 'kernel/hung_task.c'
edit(H, '''			last_break = jiffies;
		}

		if (task_is_hung(t, timeout)) {''', '''			last_break = jiffies;
		}

		/* ANDROID: 6.12 parity, vendor may exclude the task from the check */
		need_check = true;
		trace_android_vh_check_uninterruptible_tasks(t, timeout, &need_check);
		if (need_check && task_is_hung(t, timeout)) {''')
edit(H, '''			hung_task_info(t, timeout, this_round_count);
		}
	}
 unlock:''', '''			hung_task_info(t, timeout, this_round_count);
		}
	}
	trace_android_vh_check_uninterruptible_tasks_dn(NULL);
 unlock:''')
edit(H, '''	unsigned long si_mask = hung_task_si_mask;
''', '''	unsigned long si_mask = hung_task_si_mask;
	bool need_check;
''')
s = (SRC / H).read_text()
if '#include <trace/hooks/hung_task.h>' not in s:
    s = s.replace('#include <trace/events/sched.h>\n', '#include <trace/events/sched.h>\n#include <trace/hooks/hung_task.h>\n', 1)
    if '#include <trace/hooks/hung_task.h>' not in s:
        raise SystemExit('hung_task.c include anchor')
    (SRC / H).write_text(s)

edit('mm/memblock.c', '''	return (memblock.memory.regions[idx].base + memblock.memory.regions[idx].size);
}
''', '''	return (memblock.memory.regions[idx].base + memblock.memory.regions[idx].size);
}
EXPORT_SYMBOL_GPL(memblock_end_of_DRAM); /* ANDROID: 6.12 parity (mem_offline) */
''')
edit('kernel/time/sched_clock.c', '''notrace int sched_clock_read_retry(unsigned int seq)
{
	return read_seqcount_latch_retry(&cd.seq, seq);
}
''', '''notrace int sched_clock_read_retry(unsigned int seq)
{
	return read_seqcount_latch_retry(&cd.seq, seq);
}
/* ANDROID: 6.12 parity (gh_ctrl) */
EXPORT_SYMBOL_GPL(sched_clock_read_begin);
EXPORT_SYMBOL_GPL(sched_clock_read_retry);
''')
print('applied android vendor exports/hooks to', SRC)
