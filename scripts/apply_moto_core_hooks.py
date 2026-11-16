#!/usr/bin/env python3
"""NOTE: patches/moto-core-hooks-7.3.patch is authoritative; the mutex_wait_start,
rwsem_wake and rwsem_wake_finish calls were later moved outside wait_lock because
proxy execution nests wait_lock inside rq locks and vendors take rq locks there.
Vendor hooks moto_sched (canoe: MUTEX/RWSEM inherit, MDPF) needs, restored on 7.3.

Placement follows android16-6.12 (Motorola kernel-common 479a9e4ae):
  binder_proc_transaction_finish  before the wakeup at the end of binder_proc_transaction
  pr_set_vma_name_bypass          after the anon name is allocated, before mmap_lock (7.3
                                  moved the PR_SET_VMA body into set_anon_vma_name())
  percpu_rwsem_down_read/up_write, rvh percpu_rwsem_wait_complete   same points
  mutex_wait_start                right before the task marks itself blocked on the lock
  mutex_unlock_slowpath           after the next owner has been woken
The mutex list-ordering hook is the 7.3 position hook added for sched_walt.
"""
from pathlib import Path
import sys
SRC=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/'avr-upstream-audit/android-7.3')

def edit(rel,old,new):
    p=SRC/rel;s=p.read_text()
    if new in s:return
    if s.count(old)!=1:raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old,new,1))

# --- binder
edit('include/trace/hooks/binder.h','''struct binder_transaction;
struct task_struct;
''','''struct binder_transaction;
struct binder_proc;
struct task_struct;
''')
edit('include/trace/hooks/binder.h','''DECLARE_HOOK(android_vh_binder_restore_priority,''','''DECLARE_HOOK(android_vh_binder_proc_transaction_finish,
	TP_PROTO(struct binder_proc *proc, struct binder_transaction *t,
		struct task_struct *binder_th_task, bool pending_async, bool sync),
	TP_ARGS(proc, t, binder_th_task, pending_async, sync));
DECLARE_HOOK(android_vh_binder_restore_priority,''')
edit('drivers/android/binder.c','''		binder_enqueue_work_ilocked(&t->work, &node->async_todo);
	}

	if (!pending_async)
		binder_wakeup_thread_ilocked(proc, thread, !oneway /* sync */);''','''		binder_enqueue_work_ilocked(&t->work, &node->async_todo);
	}

	trace_android_vh_binder_proc_transaction_finish(proc, t,
		thread ? thread->task : NULL, pending_async, !oneway);
	if (!pending_async)
		binder_wakeup_thread_ilocked(proc, thread, !oneway /* sync */);''')

# --- prctl PR_SET_VMA_ANON_NAME
edit('include/trace/hooks/sys.h','DECLARE_HOOK(android_vh_syscall_prctl_finished,','''struct mm_struct;
struct anon_vma_name;
/*
 * Vendor may skip naming (returning *error) to avoid blocking on mmap_lock; it then
 * owns the reference to @anon_name and must drop it.
 */
DECLARE_RESTRICTED_HOOK(android_rvh_pr_set_vma_name_bypass,
	TP_PROTO(struct mm_struct *mm, unsigned long addr, unsigned long size,
		 struct anon_vma_name *anon_name, int *error, bool *bypass),
	TP_ARGS(mm, addr, size, anon_name, error, bypass), 1);
DECLARE_HOOK(android_vh_syscall_prctl_finished,''')
edit('mm/madvise.c','''	error = madvise_set_anon_name(mm, addr, size, anon_name);
	anon_vma_name_put(anon_name);''','''	{
		bool bypass = false;

		error = 0;
		trace_android_rvh_pr_set_vma_name_bypass(mm, addr, size, anon_name,
							 &error, &bypass);
		if (bypass)
			return error;
	}

	error = madvise_set_anon_name(mm, addr, size, anon_name);
	anon_vma_name_put(anon_name);''')
s=(SRC/'mm/madvise.c').read_text()
if 'trace/hooks/sys.h' not in s:
    i=s.index('#include "internal.h"')
    (SRC/'mm/madvise.c').write_text(s[:i]+'#include <trace/hooks/sys.h>\n'+s[i:])

# --- percpu rwsem
edit('include/trace/hooks/dtask.h','''DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''','''struct percpu_rw_semaphore;
DECLARE_HOOK(android_vh_percpu_rwsem_down_read,
	TP_PROTO(struct percpu_rw_semaphore *sem, bool try, bool *ret),
	TP_ARGS(sem, try, ret));
DECLARE_HOOK(android_vh_percpu_rwsem_up_write,
	TP_PROTO(struct percpu_rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_RESTRICTED_HOOK(android_rvh_percpu_rwsem_wait_complete,
	TP_PROTO(struct percpu_rw_semaphore *sem, long state, bool *complete),
	TP_ARGS(sem, state, complete), 1);
DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''')
P='kernel/locking/percpu-rwsem.c'
edit(P,'''	if (__percpu_down_read_trylock(sem))
		return true;
''','''	if (__percpu_down_read_trylock(sem))
		return true;

	{
		bool ret = false;

		trace_android_vh_percpu_rwsem_down_read(sem, try, &ret);
		if (ret)
			return true;
	}
''')
edit(P,'''	rcuwait_wait_event(&sem->writer, readers_active_check(sem), TASK_UNINTERRUPTIBLE);''','''	{
		bool complete = false;

		trace_android_rvh_percpu_rwsem_wait_complete(sem, TASK_UNINTERRUPTIBLE, &complete);
		if (!complete)
			rcuwait_wait_event(&sem->writer, readers_active_check(sem),
					   TASK_UNINTERRUPTIBLE);
	}''')
edit(P,'''void percpu_up_write(struct percpu_rw_semaphore *sem)
{
	rwsem_release(&sem->dep_map, _RET_IP_);
''','''void percpu_up_write(struct percpu_rw_semaphore *sem)
{
	rwsem_release(&sem->dep_map, _RET_IP_);

	trace_android_vh_percpu_rwsem_up_write(sem);
''')
s=(SRC/P).read_text()
if 'trace/hooks/dtask.h' not in s:
    s=s.replace('#include <trace/events/lock.h>\n','#include <trace/events/lock.h>\n#include <trace/hooks/dtask.h>\n',1)
    assert 'trace/hooks/dtask.h' in s,'percpu-rwsem include anchor'
    (SRC/P).write_text(s)

# --- mutex wait start / unlock slowpath
M='kernel/locking/mutex.c'
edit(M,'''	raw_spin_lock(&current->blocked_lock);
	__set_task_blocked_on(current, lock, BO_T_MUTEX);
	set_current_state(state);''','''	trace_android_vh_mutex_wait_start(lock);
	raw_spin_lock(&current->blocked_lock);
	__set_task_blocked_on(current, lock, BO_T_MUTEX);
	set_current_state(state);''')
edit(M,'''	if (next) {
		wake_up_process(next);
		put_task_struct(next);
	}
}''','''	if (next) {
		wake_up_process(next);
		put_task_struct(next);
	}
	trace_android_vh_mutex_unlock_slowpath(lock);
}''')

# --- exports (hook instances live in drivers/android/vendor_hooks.c)
edit('drivers/android/vendor_hooks.c','/* keep-sorted start */\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mutex_add_waiter_pos);','''/* keep-sorted start */
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_percpu_rwsem_wait_complete);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_pr_set_vma_name_bypass);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_binder_proc_transaction_finish);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mutex_unlock_slowpath);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mutex_wait_start);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_percpu_rwsem_down_read);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_percpu_rwsem_up_write);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_mutex_add_waiter_pos);''')
# --- percpu rwsem start-time (cgroup_threadgroup_rwsem protection); out-of-line helper as in 6.12
edit('include/trace/hooks/dtask.h','''DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''','''DECLARE_HOOK(android_vh_record_pcpu_rwsem_starttime,
	TP_PROTO(struct percpu_rw_semaphore *sem, unsigned long settime_jiffies),
	TP_ARGS(sem, settime_jiffies));
DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''')
H='include/linux/percpu-rwsem.h'
edit(H,'''extern void __percpu_up_read(struct percpu_rw_semaphore *sem);''','''extern void __percpu_up_read(struct percpu_rw_semaphore *sem);''')
s=(SRC/H).read_text()
if '_trace_android_vh_record_pcpu_rwsem_starttime' not in s:
    s=s.replace('static inline void percpu_down_read_internal(','''/* dtask.h cannot be included here; out-of-line wrapper as in android16-6.12 */
void _trace_android_vh_record_pcpu_rwsem_starttime(struct percpu_rw_semaphore *sem,
						   unsigned long settime);

static inline void percpu_down_read_internal(''',1)
    old='''		__percpu_down_read(sem, false, freezable); /* Unconditional memory barrier */
	/*
	 * The preempt_enable() prevents the compiler from
	 * bleeding the critical section out.
	 */
	preempt_enable();'''
    assert s.count(old)==1
    s=s.replace(old,'''		__percpu_down_read(sem, false, freezable); /* Unconditional memory barrier */
	/*
	 * The preempt_enable() prevents the compiler from
	 * bleeding the critical section out.
	 */
	_trace_android_vh_record_pcpu_rwsem_starttime(sem, jiffies);
	preempt_enable();''',1)
    old='''	if (ret)
		rwsem_acquire_read(&sem->dep_map, 0, 1, _RET_IP_);

	return ret;'''
    assert s.count(old)==1
    s=s.replace(old,'''	if (ret) {
		_trace_android_vh_record_pcpu_rwsem_starttime(sem, jiffies);
		rwsem_acquire_read(&sem->dep_map, 0, 1, _RET_IP_);
	}

	return ret;''',1)
    i=s.index('static inline void percpu_up_read(')
    j=s.index('\tpreempt_enable();\n}',i)
    s=s[:j]+'\t_trace_android_vh_record_pcpu_rwsem_starttime(sem, 0);\n'+s[j:]
    (SRC/H).write_text(s)
P='kernel/locking/percpu-rwsem.c'
edit(P,'''int __percpu_init_rwsem(struct percpu_rw_semaphore *sem,''','''void _trace_android_vh_record_pcpu_rwsem_starttime(struct percpu_rw_semaphore *sem,
						   unsigned long settime)
{
	trace_android_vh_record_pcpu_rwsem_starttime(sem, settime);
}
EXPORT_SYMBOL_GPL(_trace_android_vh_record_pcpu_rwsem_starttime);

int __percpu_init_rwsem(struct percpu_rw_semaphore *sem,''')
edit(P,'''	if (contended)
		trace_contention_end(sem, 0);
}''','''	if (contended)
		trace_contention_end(sem, 0);
	trace_android_vh_record_pcpu_rwsem_starttime(sem, jiffies);
}''')
s=(SRC/P).read_text()
if 'trace_android_vh_record_pcpu_rwsem_starttime(sem, 0);' not in s:
    i=s.index('void percpu_up_write(struct percpu_rw_semaphore *sem)')
    j=s.index('\n}\n',i)
    s=s[:j]+'\n\ttrace_android_vh_record_pcpu_rwsem_starttime(sem, 0);'+s[j:]
    (SRC/P).write_text(s)

# --- rwsem
R='kernel/locking/rwsem.c'
edit('include/trace/hooks/dtask.h','''DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''','''struct rw_semaphore;
struct rwsem_waiter;
DECLARE_HOOK(android_vh_rwsem_wake,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_wake_finish,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_read_trylock_failed,
	TP_PROTO(struct rw_semaphore *sem, long *cntp, int *ret),
	TP_ARGS(sem, cntp, ret));
/*
 * 7.3 replacement for alter_rwsem_list_add (waiter ring with first_waiter): a vendor
 * may choose the queued waiter a new *writer* goes in front of. Ignored when that
 * would displace a first waiter which already owns the handoff.
 */
DECLARE_HOOK(android_vh_rwsem_add_waiter_pos,
	TP_PROTO(struct rw_semaphore *sem, struct rwsem_waiter *waiter,
		 struct rwsem_waiter **pos),
	TP_ARGS(sem, waiter, pos));
DECLARE_HOOK(android_vh_mutex_unlock_slowpath,''')
edit(R,'''	if (!(*cntp & RWSEM_READ_FAILED_MASK)) {
		rwsem_set_reader_owned(sem);
		return true;
	}

	return false;
}''','''	if (!(*cntp & RWSEM_READ_FAILED_MASK)) {
		rwsem_set_reader_owned(sem);
		return true;
	}

	{
		int ret = 0;

		/* Vendor reader steal; @cntp already carries our READER_BIAS. */
		trace_android_vh_rwsem_read_trylock_failed(sem, cntp, &ret);
		if (ret) {
			rwsem_set_reader_owned(sem);
			return true;
		}
	}

	return false;
}''')
edit(R,'''	while (!(tmp & RWSEM_READ_FAILED_MASK)) {
		if (atomic_long_try_cmpxchg_acquire(&sem->count, &tmp,
						    tmp + RWSEM_READER_BIAS)) {
			rwsem_set_reader_owned(sem);
			ret = 1;
			break;
		}
	}
	preempt_enable();
	return ret;''','''	while (!(tmp & RWSEM_READ_FAILED_MASK)) {
		if (atomic_long_try_cmpxchg_acquire(&sem->count, &tmp,
						    tmp + RWSEM_READER_BIAS)) {
			rwsem_set_reader_owned(sem);
			ret = 1;
			break;
		}
	}

	if (!ret) {
		/* Vendor steal adds READER_BIAS itself when @cntp is NULL. */
		trace_android_vh_rwsem_read_trylock_failed(sem, NULL, &ret);
		if (ret)
			rwsem_set_reader_owned(sem);
	}
	preempt_enable();
	return ret;''')
edit(R,'''	/* we're now waiting on the lock, but no longer actively locking */
	count = atomic_long_add_return(adjustment, &sem->count);

	rwsem_cond_wake_waiter(sem, count, &wake_q);
	raw_spin_unlock_irq(&sem->wait_lock);''','''	/* we're now waiting on the lock, but no longer actively locking */
	count = atomic_long_add_return(adjustment, &sem->count);

	rwsem_cond_wake_waiter(sem, count, &wake_q);
	trace_android_vh_rwsem_wake(sem);
	raw_spin_unlock_irq(&sem->wait_lock);''')
edit(R,'''	first = sem->first_waiter;
	if (first) {
		list_add_tail(&waiter.list, &first->list);
		rwsem_cond_wake_waiter(sem, atomic_long_read(&sem->count),
				       &wake_q);''','''	first = sem->first_waiter;
	if (first) {
		struct rwsem_waiter *pos = NULL;

		trace_android_vh_rwsem_add_waiter_pos(sem, &waiter, &pos);
		if (pos == first && first->handoff_set)
			pos = NULL;	/* never displace the handoff owner */
		if (pos) {
			list_add_tail(&waiter.list, &pos->list);
			if (pos == first)
				sem->first_waiter = &waiter;
		} else {
			list_add_tail(&waiter.list, &first->list);
		}
		rwsem_cond_wake_waiter(sem, atomic_long_read(&sem->count),
				       &wake_q);''')
edit(R,'''		INIT_LIST_HEAD(&waiter.list);
		sem->first_waiter = &waiter;
		atomic_long_or(RWSEM_FLAG_WAITERS, &sem->count);
	}
''','''		INIT_LIST_HEAD(&waiter.list);
		sem->first_waiter = &waiter;
		atomic_long_or(RWSEM_FLAG_WAITERS, &sem->count);
	}
	trace_android_vh_rwsem_wake(sem);
''')
edit(R,'''	if (sem->first_waiter)
		rwsem_mark_wake(sem, RWSEM_WAKE_ANY, &wake_q);

	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
	wake_up_q(&wake_q);

	return sem;
}

/*
 * downgrade''','''	if (sem->first_waiter)
		rwsem_mark_wake(sem, RWSEM_WAKE_ANY, &wake_q);
	trace_android_vh_rwsem_wake_finish(sem);

	raw_spin_unlock_irqrestore(&sem->wait_lock, flags);
	wake_up_q(&wake_q);

	return sem;
}

/*
 * downgrade''')
s=(SRC/R).read_text()
if 'trace/hooks/dtask.h' not in s:
    s=s.replace('#include <trace/events/lock.h>\n','#include <trace/events/lock.h>\n#include <trace/hooks/dtask.h>\n',1)
    assert 'trace/hooks/dtask.h' in s,'rwsem include anchor'
    (SRC/R).write_text(s)
edit('drivers/android/vendor_hooks.c','/* keep-sorted start */\nEXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_percpu_rwsem_wait_complete);','''/* keep-sorted start */
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_record_pcpu_rwsem_starttime);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_add_waiter_pos);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_read_trylock_failed);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_wake);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_vh_rwsem_wake_finish);
EXPORT_TRACEPOINT_SYMBOL_GPL(android_rvh_percpu_rwsem_wait_complete);''')
print('applied moto_sched core hooks to',SRC)
