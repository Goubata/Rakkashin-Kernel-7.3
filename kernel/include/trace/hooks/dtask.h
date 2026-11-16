/* SPDX-License-Identifier: GPL-2.0 */
#undef TRACE_SYSTEM
#define TRACE_SYSTEM dtask
#define TRACE_INCLUDE_PATH trace/hooks

#if !defined(_TRACE_HOOK_DTASK_H) || defined(TRACE_HEADER_MULTI_READ)
#define _TRACE_HOOK_DTASK_H

#include <trace/hooks/vendor_hooks.h>

/*
 * Following tracepoints are not exported in tracefs and provide a
 * mechanism for vendor modules to hook and extend functionality
 */

struct mutex;
struct mutex_waiter;
struct rt_mutex_base;
struct rw_semaphore;

DECLARE_HOOK(android_vh_alter_mutex_list_add,
	TP_PROTO(struct mutex *lock,
		struct mutex_waiter *waiter,
		struct list_head *list,
		bool *already_on_list),
	TP_ARGS(lock, waiter, list, already_on_list));
/*
 * 7.3 replacement for alter_mutex_list_add: the vendor may only choose the waiter
 * that @waiter is inserted in front of (*pos stays NULL for FIFO tail).
 */
DECLARE_HOOK(android_vh_mutex_add_waiter_pos,
	TP_PROTO(struct mutex *lock, struct mutex_waiter *waiter, struct mutex_waiter **pos),
	TP_ARGS(lock, waiter, pos));
struct percpu_rw_semaphore;
DECLARE_HOOK(android_vh_percpu_rwsem_down_read,
	TP_PROTO(struct percpu_rw_semaphore *sem, bool try, bool *ret),
	TP_ARGS(sem, try, ret));
DECLARE_HOOK(android_vh_percpu_rwsem_up_write,
	TP_PROTO(struct percpu_rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_RESTRICTED_HOOK(android_rvh_percpu_rwsem_wait_complete,
	TP_PROTO(struct percpu_rw_semaphore *sem, long state, bool *complete),
	TP_ARGS(sem, state, complete), 1);
DECLARE_HOOK(android_vh_record_pcpu_rwsem_starttime,
	TP_PROTO(struct percpu_rw_semaphore *sem, unsigned long settime_jiffies),
	TP_ARGS(sem, settime_jiffies));
struct rw_semaphore;
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
 * may choose the queued waiter a new *writer* goes in front of; becoming the new
 * head also takes over the head's handoff (6.12 behaviour).
 */
DECLARE_HOOK(android_vh_rwsem_add_waiter_pos,
	TP_PROTO(struct rw_semaphore *sem, struct rwsem_waiter *waiter,
		 struct rwsem_waiter **pos),
	TP_ARGS(sem, waiter, pos));
DECLARE_HOOK(android_vh_mutex_unlock_slowpath,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));
DECLARE_RESTRICTED_HOOK(android_rvh_dpm_prepare,
	TP_PROTO(int flag),
	TP_ARGS(flag), 1);

DECLARE_HOOK(android_vh_mutex_wait_start,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_mutex_wait_finish,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_mutex_init,
	TP_PROTO(struct mutex *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_rtmutex_wait_start,
	TP_PROTO(struct rt_mutex_base *lock),
	TP_ARGS(lock));
DECLARE_HOOK(android_vh_rtmutex_wait_finish,
	TP_PROTO(struct rt_mutex_base *lock),
	TP_ARGS(lock));

DECLARE_HOOK(android_vh_rwsem_read_wait_start,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_read_wait_finish,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_write_wait_start,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));
DECLARE_HOOK(android_vh_rwsem_write_wait_finish,
	TP_PROTO(struct rw_semaphore *sem),
	TP_ARGS(sem));

struct task_struct;
struct rq;
DECLARE_HOOK(android_vh_sched_show_task,
	TP_PROTO(struct task_struct *task),
	TP_ARGS(task));

DECLARE_HOOK(android_vh_set_tsk_need_resched_lazy,
	TP_PROTO(struct task_struct *p, struct rq *rq, int *need_lazy),
	TP_ARGS(p, rq, need_lazy));
#endif /* _TRACE_HOOK_DTASK_H */
/* This part must be outside protection */
#include <trace/define_trace.h>
