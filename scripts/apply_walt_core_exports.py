#!/usr/bin/env python3
"""Core interfaces sched_walt links against, restored for android18-7.3.

android16-6.12 exported pick_task(rq), __balance_callbacks, cpu_busy_with_softirqs and
ext_sched_class, kept vendor/OEM data in struct binder_transaction and offered
android_vh_alter_mutex_list_add on a list-headed waiter queue. 7.3 rewrote the mutex
wait queue (first_waiter + headless ring) and gave __mutex_add_waiter() an insertion
position, so the vendor hook now only *chooses* the position; insertion, first-waiter
and flag updates stay in the native code that ww_mutex already uses.
"""
from pathlib import Path
import sys
SRC=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/'avr-upstream-audit/android-7.3')

def edit(rel,old,new):
    p=SRC/rel;s=p.read_text()
    if new in s:return
    if s.count(old)!=1:raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old,new,1))

edit('drivers/android/binder_internal.h','''	spinlock_t lock;
};

/**
 * struct binder_object''','''	spinlock_t lock;
	/* ANDROID: vendor (sched_walt) and OEM data, as in android16-6.12 */
	ANDROID_VENDOR_DATA(1);
	ANDROID_OEM_DATA(1);
};

/**
 * struct binder_object''')

edit('kernel/sched/core.c','''void balance_callbacks(struct rq *rq, struct balance_callback *head)''','''EXPORT_SYMBOL_GPL(__balance_callbacks);

void balance_callbacks(struct rq *rq, struct balance_callback *head)''')

edit('kernel/sched/core.c','''extern void task_vruntime_update(struct rq *rq, struct task_struct *p, bool in_fi);''','''extern void task_vruntime_update(struct rq *rq, struct task_struct *p, bool in_fi);
#endif /* CONFIG_SCHED_CORE */

/*
 * ANDROID: pick the next task without switching to it (android16-6.12 exported
 * pick_task(rq) for sched_walt CPU halt). Caller holds the pinned rq lock and @rf.
 */
struct task_struct *android_pick_task(struct rq *rq, struct rq_flags *rf)
{
	const struct sched_class *class;
	struct task_struct *p;

restart:
	rq->dl_server = NULL;
	for_each_active_class(class) {
		p = class->pick_task(rq, rf);
		if (unlikely(p == RETRY_TASK))
			goto restart;
		if (p)
			return p;
	}

	BUG(); /* The idle class should always have a runnable task. */
}
EXPORT_SYMBOL_GPL(android_pick_task);

#ifdef CONFIG_SCHED_CORE''')

edit('kernel/sched/rt.c','''static bool cpu_busy_with_softirqs(int cpu)
{
	u32 softirqs = per_cpu(active_softirqs, cpu) |''','''bool cpu_busy_with_softirqs(int cpu)
{
	u32 softirqs = per_cpu(active_softirqs, cpu) |''')
edit('kernel/sched/rt.c','''	return softirqs & LONG_SOFTIRQ_MASK;
}''','''	return softirqs & LONG_SOFTIRQ_MASK;
}
EXPORT_SYMBOL_GPL(cpu_busy_with_softirqs);''')

edit('kernel/sched/ext/ext.c','DEFINE_STATIC_KEY_FALSE(__scx_enabled);\n','DEFINE_STATIC_KEY_FALSE(__scx_enabled);\nEXPORT_SYMBOL_GPL(__scx_enabled);\n')
s=(SRC/'kernel/sched/ext/ext.c').read_text()
if 'EXPORT_SYMBOL_GPL(ext_sched_class)' not in s:
    i=s.index('DEFINE_SCHED_CLASS(ext) = {');j=s.index('\n};\n',i)+4
    (SRC/'kernel/sched/ext/ext.c').write_text(s[:j]+'EXPORT_SYMBOL_GPL(ext_sched_class);\n'+s[j:])

edit('include/trace/hooks/dtask.h','''DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''','''/*
 * 7.3 replacement for alter_mutex_list_add: the vendor may only choose the waiter
 * that @waiter is inserted in front of (*pos stays NULL for FIFO tail).
 */
DECLARE_HOOK(android_vh_mutex_add_waiter_pos,
	TP_PROTO(struct mutex *lock, struct mutex_waiter *waiter, struct mutex_waiter **pos),
	TP_ARGS(lock, waiter, pos));
DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''')
edit('kernel/locking/mutex.c','''	if (!use_ww_ctx) {
		/* add waiting tasks to the end of the waitqueue (FIFO): */
		__mutex_add_waiter(lock, &waiter, NULL);''','''	if (!use_ww_ctx) {
		struct mutex_waiter *pos = NULL;

		/* FIFO tail unless a vendor ordering policy picks a waiter to precede. */
		trace_android_vh_mutex_add_waiter_pos(lock, &waiter, &pos);
		__mutex_add_waiter(lock, &waiter, pos);''')
s=(SRC/'kernel/locking/mutex.c').read_text()
if 'trace/hooks/dtask.h' not in s:
    # before CREATE_TRACE_POINTS: the hook instances live in drivers/android/vendor_hooks.c
    edit('kernel/locking/mutex.c','#define CREATE_TRACE_POINTS\n#include <trace/events/lock.h>\n','#define CREATE_TRACE_POINTS\n#include <trace/events/lock.h>\n#undef CREATE_TRACE_POINTS\n/* hook instances are created in drivers/android/vendor_hooks.c */\n#include <trace/hooks/dtask.h>\n')
edit('drivers/android/vendor_hooks.c','/* keep-sorted start */\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_restore_priority);','''/* keep-sorted start */
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mutex_add_waiter_pos);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_restore_priority);''')
print('applied WALT core exports to',SRC)
