// SPDX-License-Identifier: GPL-2.0-only
/*
 * Real moto_sched (canoe: MUTEX/RWSEM inherit) on the restored 7.3 hooks, with real
 * sched_walt underneath and proxy execution enabled. UX state is set exactly where
 * moto_sched keeps it (task_struct OEM data). /proc/moto_sched/enabled is set by the
 * initramfs before this module loads.
 */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/percpu-rwsem.h>
#include <linux/rwsem.h>
#include <linux/sched.h>
#include <linux/sched/debug.h>
#include <uapi/linux/sched/types.h>
#include "../../ports/moto-sched/msched_common.h"

static DEFINE_MUTEX(tm);
static DECLARE_RWSEM(trw);
DEFINE_STATIC_PERCPU_RWSEM(tpc);

struct worker {
	struct task_struct *t;
	struct completion started, acquired, release, done;
	int kind;		/* 0 mutex, 1 rwsem write, 2 rwsem read, 3 percpu read */
	bool ux, hold;
	atomic_t *order;
	int my_order;
};

static void set_ux(struct task_struct *t, bool on)
{
	struct moto_task_struct *mts = get_moto_task_struct(t);

	if (on)
		mts->ux_type |= UX_TYPE_INPUT;
	else
		mts->ux_type &= ~UX_TYPE_INPUT;
}

static int worker_fn(void *data)
{
	struct worker *w = data;

	set_ux(current, w->ux);
	complete(&w->started);
	switch (w->kind) {
	case 0: mutex_lock(&tm); break;
	case 1: down_write(&trw); break;
	case 2: down_read(&trw); break;
	case 3: percpu_down_read(&tpc); break;
	}
	complete_all(&w->acquired);
	if (w->order)
		w->my_order = atomic_inc_return(w->order);
	if (w->hold)
		wait_for_completion(&w->release);
	switch (w->kind) {
	case 0: mutex_unlock(&tm); break;
	case 1: up_write(&trw); break;
	case 2: up_read(&trw); break;
	case 3: percpu_up_read(&tpc); break;
	}
	complete_all(&w->done);	/* waited both by the case and by finish() */
	while (!kthread_should_stop())
		schedule_timeout_interruptible(1);
	return 0;
}

static struct worker *start(struct kunit *test, int kind, bool ux, bool hold, atomic_t *order)
{
	struct worker *w = kunit_kzalloc(test, sizeof(*w), GFP_KERNEL);

	KUNIT_ASSERT_NOT_NULL(test, w);
	init_completion(&w->started);
	init_completion(&w->acquired);
	init_completion(&w->release);
	init_completion(&w->done);
	w->kind = kind;
	w->ux = ux;
	w->hold = hold;
	w->order = order;
	w->t = kthread_run(worker_fn, w, "avr-moto-%d", kind);
	KUNIT_ASSERT_FALSE(test, IS_ERR(w->t));
	get_task_struct(w->t);
	wait_for_completion(&w->started);
	/* a holding owner must own the lock before any waiter is started */
	if (hold && !wait_for_completion_timeout(&w->acquired, 10 * HZ))
		KUNIT_FAIL(test, "owner did not acquire the lock");
	return w;
}

static bool finish_failed;

static void finish(struct worker *w)
{
	complete(&w->release);
	if (!wait_for_completion_timeout(&w->done, 10 * HZ)) {
		pr_err("AVR_MOTO_STUCK kind=%d pid=%d state=0x%x on_rq=%d on_cpu=%d blocked=%d mutex_owner=%px\n",
		       w->kind, w->t->pid, READ_ONCE(w->t->__state), w->t->on_rq, w->t->on_cpu,
		       w->t->is_blocked, (void *)(atomic_long_read(&tm.owner) & ~0x7UL));
		sched_show_task(w->t);
		finish_failed = true;
		return;	/* leak the worker; the suite is failed anyway */
	}
	kthread_stop(w->t);
	put_task_struct(w->t);
}

static bool wait_depth(struct task_struct *t, bool want)
{
	int i;

	for (i = 0; i < 200; i++) {
		if (!!task_get_ux_depth(t) == want)
			return true;
		msleep(10);
	}
	return false;
}

static bool wait_blocked_timed_out;

/* Up to 10 s: under host load a worker can take >1 s to reach the lock's wait
 * list; proceeding early would test the wrong ordering, so report it. */
static void wait_blocked(struct task_struct *t)
{
	int i;

	for (i = 0; i < 2000 && READ_ONCE(t->__state) == TASK_RUNNING; i++)
		msleep(5);
	if (READ_ONCE(t->__state) == TASK_RUNNING) {
		pr_err("AVR_MOTO_NOT_BLOCKED pid=%d\n", t->pid);
		wait_blocked_timed_out = true;
	}
	msleep(20);
}

static void mutex_inherit_and_clear(struct kunit *test)
{
	struct worker *owner = start(test, 0, false, true, NULL);
	struct worker *waiter;

	msleep(20);
	waiter = start(test, 0, true, false, NULL);
	wait_blocked(waiter->t);
	KUNIT_EXPECT_TRUE(test, wait_depth(owner->t, true));
	KUNIT_EXPECT_TRUE(test, get_moto_task_struct(owner->t)->ux_type & UX_TYPE_INHERIT_LOCK);
	finish(owner);
	wait_for_completion(&waiter->done);
	KUNIT_EXPECT_TRUE(test, wait_depth(owner->t, false));
	finish(waiter);
}

static void mutex_ux_waiter_jumps_queue(struct kunit *test)
{
	atomic_t order = ATOMIC_INIT(0);
	struct worker *owner = start(test, 0, false, true, NULL);
	struct worker *plain, *ux;

	msleep(20);
	plain = start(test, 0, false, false, &order);
	wait_blocked(plain->t);
	ux = start(test, 0, true, false, &order);
	wait_blocked(ux->t);
	KUNIT_EXPECT_FALSE(test, wait_blocked_timed_out);
	finish(owner);
	wait_for_completion(&plain->done);
	wait_for_completion(&ux->done);
	KUNIT_EXPECT_EQ(test, ux->my_order, 1);
	KUNIT_EXPECT_EQ(test, plain->my_order, 2);
	finish(plain);
	finish(ux);
}

static void rwsem_inherit_and_clear(struct kunit *test)
{
	struct worker *owner = start(test, 1, false, true, NULL);
	struct worker *waiter;

	msleep(20);
	waiter = start(test, 1, true, false, NULL);
	wait_blocked(waiter->t);
	KUNIT_EXPECT_TRUE(test, wait_depth(owner->t, true));
	finish(owner);
	wait_for_completion(&waiter->done);
	KUNIT_EXPECT_TRUE(test, wait_depth(owner->t, false));
	finish(waiter);
}

static void rwsem_ux_writer_jumps_queue(struct kunit *test)
{
	atomic_t order = ATOMIC_INIT(0);
	struct worker *owner = start(test, 1, false, true, NULL);
	struct worker *plain, *ux;

	msleep(20);
	plain = start(test, 1, false, false, &order);
	wait_blocked(plain->t);
	ux = start(test, 1, true, false, &order);
	wait_blocked(ux->t);
	KUNIT_EXPECT_FALSE(test, wait_blocked_timed_out);
	finish(owner);
	wait_for_completion(&plain->done);
	wait_for_completion(&ux->done);
	KUNIT_EXPECT_EQ(test, ux->my_order, 1);
	KUNIT_EXPECT_EQ(test, plain->my_order, 2);
	finish(plain);
	finish(ux);
}

/* RT readers may steal a reader-owned rwsem past a queued writer (6.12 policy). */
static int rt_trylock_fn(void *data)
{
	int *res = data;
	struct sched_param sp = { .sched_priority = 10 };

	sched_setscheduler_nocheck(current, SCHED_FIFO, &sp);
	*res = down_read_trylock(&trw);
	if (*res)
		up_read(&trw);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(1);
	return 0;
}

static void rwsem_rt_reader_steal(struct kunit *test)
{
	struct worker *reader = start(test, 2, false, true, NULL);
	struct worker *writer;
	struct task_struct *t;
	int res = -1;

	msleep(20);
	writer = start(test, 1, false, false, NULL);
	wait_blocked(writer->t);
	KUNIT_EXPECT_FALSE(test, down_read_trylock(&trw));	/* plain reader: no steal */
	t = kthread_run(rt_trylock_fn, &res, "avr-moto-rt");
	KUNIT_ASSERT_FALSE(test, IS_ERR(t));
	while (READ_ONCE(res) < 0)
		msleep(5);
	kthread_stop(t);
	KUNIT_EXPECT_EQ(test, res, 1);
	finish(reader);
	wait_for_completion(&writer->done);
	finish(writer);
}

static void percpu_rwsem_with_hooks(struct kunit *test)
{
	struct worker *r[4];
	int i, n;

	for (n = 0; n < 20; n++) {
		for (i = 0; i < 4; i++)
			r[i] = start(test, 3, i & 1, false, NULL);
		percpu_down_write(&tpc);
		percpu_up_write(&tpc);
		for (i = 0; i < 4; i++)
			finish(r[i]);
	}
	KUNIT_EXPECT_TRUE(test, true);
}

/* Contention storm with proxy execution: must neither deadlock nor warn. */
static int storm_fn(void *data)
{
	unsigned long end = jiffies + msecs_to_jiffies(1500);
	int i = (long)data;

	set_ux(current, i & 1);
	while (time_before(jiffies, end)) {
		mutex_lock(&tm);
		udelay(50);
		mutex_unlock(&tm);
		if (i & 2) {
			down_write(&trw);
			udelay(30);
			up_write(&trw);
		} else {
			down_read(&trw);
			udelay(30);
			up_read(&trw);
		}
		cond_resched();
	}
	while (!kthread_should_stop())
		schedule_timeout_interruptible(1);
	return 0;
}

static void lock_storm(struct kunit *test)
{
	struct task_struct *t[8];
	int i;

	for (i = 0; i < 8; i++) {
		t[i] = kthread_run(storm_fn, (void *)(long)i, "avr-moto-storm%d", i);
		KUNIT_ASSERT_FALSE(test, IS_ERR(t[i]));
	}
	msleep(2500);
	for (i = 0; i < 8; i++)
		kthread_stop(t[i]);
	KUNIT_EXPECT_EQ(test, task_get_ux_depth(current), 0);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(mutex_inherit_and_clear), KUNIT_CASE(mutex_ux_waiter_jumps_queue),
	KUNIT_CASE(rwsem_inherit_and_clear), KUNIT_CASE(rwsem_ux_writer_jumps_queue),
	KUNIT_CASE(rwsem_rt_reader_steal), KUNIT_CASE(percpu_rwsem_with_hooks),
	KUNIT_CASE(lock_storm), {}
};
static struct kunit_suite suite = { .name = "avr-moto-sched", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
