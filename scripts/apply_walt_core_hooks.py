#!/usr/bin/env python3
"""Add the five vendor hooks sched_walt needs that android18-7.3 dropped.

Placement follows android16-6.12 (Motorola kernel-common 479a9e4ae) semantics,
mapped onto the 7.3 EEVDF code: wakeup_preempt_fair (was check_preempt_wakeup_fair),
pick_task_fair (pick_next_task_fair was folded into it), find_new_ilb, and binder
priority set/restore. preemptirq/scx hooks are only used by sched-walt-debug /
sched_ext paths that the phone does not load, so they are intentionally omitted.
"""
from pathlib import Path
import sys
SRC=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/'avr-upstream-audit/android-7.3')

def edit(rel,old,new):
    p=SRC/rel;s=p.read_text()
    if new in s:return
    if s.count(old)!=1:raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old,new,1))

edit('include/trace/hooks/sched.h','''DECLARE_RESTRICTED_HOOK(android_rvh_update_misfit_status,''','''DECLARE_RESTRICTED_HOOK(android_rvh_check_preempt_wakeup_fair,
	TP_PROTO(struct rq *rq, struct task_struct *p, bool *preempt, bool *nopreempt,
			int wake_flags, struct sched_entity *se, struct sched_entity *pse),
	TP_ARGS(rq, p, preempt, nopreempt, wake_flags, se, pse), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_replace_next_task_fair,
	TP_PROTO(struct rq *rq, struct task_struct **p, struct task_struct *prev),
	TP_ARGS(rq, p, prev), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_find_new_ilb,
	TP_PROTO(struct cpumask *nohz_idle_cpus_mask, int *new_ilb),
	TP_ARGS(nohz_idle_cpus_mask, new_ilb), 1);

DECLARE_RESTRICTED_HOOK(android_rvh_update_misfit_status,''')

# 6.12 called the hook after the idle/policy filters and update_curr, before the
# slice/eligibility decision; preempt -> resched, nopreempt -> keep current.
edit('kernel/sched/fair.c','''	/*
	 * Do not preempt for tasks that are sched_delayed as it would violate
	 * EEVDF to forcibly queue an ineligible task.
	 */
	if (pse->sched_delayed)
		goto update;
''','''	/*
	 * Do not preempt for tasks that are sched_delayed as it would violate
	 * EEVDF to forcibly queue an ineligible task.
	 */
	if (pse->sched_delayed)
		goto update;

	{
		bool vendor_preempt = false, vendor_ignore = false;

		trace_android_rvh_check_preempt_wakeup_fair(rq, p, &vendor_preempt,
							    &vendor_ignore, wake_flags, se, pse);
		if (vendor_preempt)
			goto preempt;
		if (vendor_ignore)
			goto update;
	}
''')

# Vendor may substitute a runnable fair task of this rq (MVP); rq->curr is prev.
edit('kernel/sched/fair.c','''	se = pick_next_entity(rq, true);
	if (!se)
		goto again;

	p = task_of(se);
	return p;
''','''	se = pick_next_entity(rq, true);
	if (!se)
		goto again;

	p = task_of(se);
	trace_android_rvh_replace_next_task_fair(rq, &p, rq->curr);
	return p;
''')

# Vendor returns a CPU, or -1 for "none" (6.12 contract); nr_cpu_ids = defer.
edit('kernel/sched/fair.c','''	lockdep_assert_irqs_disabled();

	/*
	 * Reuse the per-CPU select_rq_mask, which is protected from concurrent
	 * use on this CPU by having interrupts disabled.
	 */''','''	lockdep_assert_irqs_disabled();

	{
		int new_ilb = nr_cpu_ids;

		trace_android_rvh_find_new_ilb(nohz.idle_cpus_mask, &new_ilb);
		if (new_ilb != nr_cpu_ids)
			return new_ilb;
	}

	/*
	 * Reuse the per-CPU select_rq_mask, which is protected from concurrent
	 * use on this CPU by having interrupts disabled.
	 */''')
# Scheduler hook exports live with the other sched hooks.
edit('kernel/sched/vendor_hooks.c','EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_update_misfit_status);\n','''EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_update_misfit_status);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_check_preempt_wakeup_fair);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_replace_next_task_fair);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_find_new_ilb);
''')

# Binder priority hooks (WALT tracks binder-inherited priority for MVP).
binder_h=SRC/'include/trace/hooks/binder.h'
if not binder_h.exists():
    binder_h.write_text('''/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM binder
#undef TRACE_INCLUDE_PATH
#define TRACE_INCLUDE_PATH trace/hooks
#if !defined(_TRACE_HOOK_BINDER_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_BINDER_H
#include <trace/hooks/vendor_hooks.h>
/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */
struct binder_transaction;
struct task_struct;

DECLARE_HOOK(android_vh_binder_set_priority,
	TP_PROTO(struct binder_transaction *t, struct task_struct *task),
	TP_ARGS(t, task));
DECLARE_HOOK(android_vh_binder_restore_priority,
	TP_PROTO(struct binder_transaction *t, struct task_struct *task),
	TP_ARGS(t, task));

#endif /* _TRACE_HOOK_BINDER_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
''')
edit('drivers/android/vendor_hooks.c','#include <trace/hooks/avc.h>\n','#include <trace/hooks/avc.h>\n#include <trace/hooks/binder.h>\n')
edit('drivers/android/vendor_hooks.c','/* keep-sorted start */\nEXPORT_TRACEPOINT_SYMBOL_GPL(','''/* keep-sorted start */
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_restore_priority);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_set_priority);
EXPORT_TRACEPOINT_SYMBOL_GPL(''')
b=SRC/'drivers/android/binder.c';s=b.read_text()
if 'trace/hooks/binder.h' not in s:
    s=s.replace('#include "binder_trace.h"\n','#include "binder_trace.h"\n#include <trace/hooks/binder.h>\n',1)
    assert 'trace/hooks/binder.h' in s,'binder.c include anchor'
    old='''	binder_set_priority(thread, &desired);
}
'''
    assert s.count(old)==1
    s=s.replace(old,'''	binder_set_priority(thread, &desired);
	trace_android_vh_binder_set_priority(t, task);
}
''',1)
    # 6.12 fired restore before each binder_restore_priority(thread, ...).
    n=0
    for old,new in [('		binder_restore_priority(thread, &in_reply_to->saved_priority);\n','		trace_android_vh_binder_restore_priority(in_reply_to, current);\n		binder_restore_priority(thread, &in_reply_to->saved_priority);\n'),
                    ('		binder_restore_priority(thread, &proc->default_priority);\n','		trace_android_vh_binder_restore_priority(NULL, current);\n		binder_restore_priority(thread, &proc->default_priority);\n')]:
        n+=s.count(old);s=s.replace(old,new)
    assert n==3,f'binder restore sites {n}'
    b.write_text(s)
print('applied WALT core hooks to',SRC)
