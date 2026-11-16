// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * 7.3 port: the mutex wait queue is first_waiter + a headless ring and the core
 * inserts at a vendor-chosen position. MVP waiters still go in front of the first
 * non-MVP waiter (6.12 alter_mutex_list_add policy); the core does the insertion.
 */

#include <trace/hooks/dtask.h>
#include "kernel/locking/mutex.h"
#include "walt.h"

static void android_vh_mutex_add_waiter_pos(void *unused, struct mutex *lock,
				struct mutex_waiter *waiter, struct mutex_waiter **pos)
{
	struct walt_task_struct *wts_waiter =
		(struct walt_task_struct *)android_task_vendor_data(current);
	struct mutex_waiter *first, *cur;
	struct walt_task_struct *wts;

	if (unlikely(walt_disabled))
		return;

	if (!lock || !waiter || !pos)
		return;

	if (!is_mvp(wts_waiter))
		return;

	first = lock->first_waiter;
	if (!first)
		return;
	cur = first;
	do {
		wts = (struct walt_task_struct *)
			android_task_vendor_data((struct task_struct *)(cur->task));
		if (!is_mvp(wts)) {
			*pos = cur;
			return;
		}
		cur = list_next_entry(cur, list);
	} while (cur != first);
}

void walt_mvp_lock_ordering_init(void)
{
	register_trace_android_vh_mutex_add_waiter_pos(android_vh_mutex_add_waiter_pos, NULL);
}
