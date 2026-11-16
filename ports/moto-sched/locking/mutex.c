// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (C) 2024 Moto. All rights reserved.
 */

#include <linux/mutex.h>
#include <linux/sched/task.h>
#include <linux/ww_mutex.h>
#include <linux/version.h>
#include <trace/hooks/dtask.h>
#include <linux/stacktrace.h>

#include "../msched_common.h"
#include "locking_main.h"

/*
 * Note:
 * The following structure must be the same as it's definition in kernel/locking/mutex.h
 */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
struct mutex_waiter {
	struct list_head	list;
	struct task_struct	*task;
	struct ww_acquire_ctx	*ww_ctx;
#ifdef CONFIG_DEBUG_MUTEXES
	void			*magic;
#endif
};
#endif

#define MUTEX_FLAGS		0x07
static inline struct task_struct *__mutex_owner(struct mutex *lock)
{
	return (struct task_struct *)(atomic_long_read(&lock->owner) & ~MUTEX_FLAGS);
}

/*
 * 7.3 port: the mutex queue is first_waiter + a headless ring and the core inserts
 * at a vendor-chosen position. Policy unchanged from 6.12: an important-UX waiter
 * goes in front of the first non-RT waiter with a lower MVP priority.
 */
static struct mutex_waiter *mutex_pick_pos(struct mutex *lock)
{
	struct mutex_waiter *first = lock->first_waiter, *cur;
	int prio = task_get_mvp_prio(current, true);
	int index = 0;

	if (!first)
		return NULL;
	cur = first;
	do {
		if (cur->task && cur->task->prio > MAX_RT_PRIO &&
		    prio > task_get_mvp_prio(cur->task, true)) {
			cond_trace_printk(unlikely(is_debuggable(DEBUG_BASE)),
				"mutex_list_add_ux %d  prio=%d(%d)index=%d\n", current->pid, prio,
				task_get_mvp_prio(cur->task, true), index);
			return cur;
		}
		index++;
		cur = list_next_entry(cur, list);
	} while (cur != first);
	return NULL;
}

/* implement vender hook in kernel/locking/mutex.c */
static void android_vh_alter_mutex_list_add_handler(void *unused, struct mutex *lock,
			struct mutex_waiter *waiter, struct mutex_waiter **pos)
{
	if (unlikely(!locking_opt_enable()) || !lock || !pos)
		return;

	if (task_is_important_ux(current))
		*pos = mutex_pick_pos(lock);
}

static void android_vh_mutex_wait_start_handler(void *unused, struct mutex *lock)
{
	struct task_struct *owner_ts = NULL;
	bool boost = false;

	if (unlikely(!locking_opt_enable() || !lock)) {
		return;
	}

	if ((current->prio >= 100) && (!current_is_important_ux())) {
		return;
	}

	owner_ts = __mutex_owner(lock);
	if (!owner_ts) {
		cond_trace_printk(unlikely(is_debuggable(DEBUG_BASE)),
				"mutex owner not found!!! =%d\n", current->pid);
		return;
	}

	get_task_struct(owner_ts);
	boost = lock_inherit_ux_type(owner_ts, current, "mutex_wait");

	if (boost && (__mutex_owner(lock) != owner_ts)) {
		cond_trace_printk(unlikely(is_debuggable(DEBUG_BASE)),
			"mutex owner has been changed owner=%p(%p)\n", __mutex_owner(lock), owner_ts);
		lock_clear_inherited_ux_type(owner_ts, "mutex_wait");
	}
	put_task_struct(owner_ts);
}

void android_vh_mutex_unlock_slowpath_handler(void *unused, struct mutex *lock)
{
	if (unlikely(!locking_opt_enable()))
		return;

	lock_clear_inherited_ux_type(current, "mutex_wait_finish");
}

void register_mutex_vendor_hooks(void)
{
	register_trace_android_vh_mutex_add_waiter_pos(android_vh_alter_mutex_list_add_handler, NULL);
	register_trace_android_vh_mutex_wait_start(android_vh_mutex_wait_start_handler, NULL);
	register_trace_android_vh_mutex_unlock_slowpath(android_vh_mutex_unlock_slowpath_handler, NULL);
}

void unregister_mutex_vendor_hooks(void)
{
	unregister_trace_android_vh_mutex_add_waiter_pos(android_vh_alter_mutex_list_add_handler, NULL);
	unregister_trace_android_vh_mutex_wait_start(android_vh_mutex_wait_start_handler, NULL);
	unregister_trace_android_vh_mutex_unlock_slowpath(android_vh_mutex_unlock_slowpath_handler, NULL);
}
