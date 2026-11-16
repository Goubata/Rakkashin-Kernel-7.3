// SPDX-License-Identifier: GPL-2.0-only
/*
 * Probes on the five vendor hooks restored for sched_walt, attached exactly as a
 * vendor module would (register_trace_android_*). Scheduler cases drive real
 * wakeups/picks/nohz kicks on QEMU CPUs; binder hook counters are exposed as
 * read-only parameters and checked after the userspace binder-pi transaction.
 */
#include <kunit/test.h>
#include <linux/atomic.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <trace/hooks/binder.h>
#include <trace/hooks/sched.h>

static atomic_t wake_calls, wake_match, pick_calls, pick_bad, ilb_calls;
static int binder_set, binder_restore;
module_param(binder_set, int, 0444);
module_param(binder_restore, int, 0444);
static struct task_struct *watched;

static void p_wake(void *d, struct rq *rq, struct task_struct *p, bool *preempt, bool *nopreempt,
		   int wake_flags, struct sched_entity *se, struct sched_entity *pse)
{
	atomic_inc(&wake_calls);
	if (p == READ_ONCE(watched) && pse == &p->se && !*preempt && !*nopreempt)
		atomic_inc(&wake_match);
}

static void p_pick(void *d, struct rq *rq, struct task_struct **p, struct task_struct *prev)
{
	atomic_inc(&pick_calls);
	if (*p && (!READ_ONCE((*p)->on_rq) || task_cpu(*p) != smp_processor_id()))
		atomic_inc(&pick_bad);
}

static void p_ilb(void *d, struct cpumask *idle, int *ilb)
{
	atomic_inc(&ilb_calls);	/* leave nr_cpu_ids: native selection continues */
}

static void p_bset(void *d, struct binder_transaction *t, struct task_struct *task)
{
	WRITE_ONCE(binder_set, binder_set + 1);
}

static void p_brestore(void *d, struct binder_transaction *t, struct task_struct *task)
{
	WRITE_ONCE(binder_restore, binder_restore + 1);
}

static int spin_fn(void *data)
{
	unsigned long end = jiffies + msecs_to_jiffies((unsigned long)data);

	while (time_before(jiffies, end) && !kthread_should_stop())
		cpu_relax();
	while (!kthread_should_stop())
		schedule_timeout_interruptible(1);
	return 0;
}

static int sleeper_fn(void *data)
{
	int i;

	for (i = 0; i < 50 && !kthread_should_stop(); i++)
		usleep_range(200, 400);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(1);
	return 0;
}

static struct task_struct *pinned(int (*fn)(void *), void *arg, int cpu, const char *name)
{
	struct task_struct *t = kthread_create(fn, arg, "%s", name);

	if (!IS_ERR(t)) {
		kthread_bind(t, cpu);
		get_task_struct(t);
	}
	return t;
}

static void stop(struct task_struct *t)
{
	kthread_stop(t);
	put_task_struct(t);
}

/* A fair sleeper repeatedly waking on a CPU busy with a fair spinner. */
static void wakeup_preempt_hook(struct kunit *test)
{
	struct task_struct *hog = pinned(spin_fn, (void *)300UL, 1, "avr-hog");
	struct task_struct *sl = pinned(sleeper_fn, NULL, 1, "avr-sleeper");

	KUNIT_ASSERT_FALSE(test, IS_ERR(hog));
	KUNIT_ASSERT_FALSE(test, IS_ERR(sl));
	WRITE_ONCE(watched, sl);
	wake_up_process(hog);
	msleep(5);
	wake_up_process(sl);
	msleep(400);
	WRITE_ONCE(watched, NULL);
	stop(sl);
	stop(hog);
	KUNIT_EXPECT_GT(test, atomic_read(&wake_calls), 0);
	KUNIT_EXPECT_GT(test, atomic_read(&wake_match), 0);
}

static void pick_hook(struct kunit *test)
{
	struct task_struct *a = pinned(spin_fn, (void *)100UL, 2, "avr-pick-a");
	struct task_struct *b = pinned(spin_fn, (void *)100UL, 2, "avr-pick-b");
	int before = atomic_read(&pick_calls);

	KUNIT_ASSERT_FALSE(test, IS_ERR(a));
	KUNIT_ASSERT_FALSE(test, IS_ERR(b));
	wake_up_process(a);
	wake_up_process(b);
	msleep(200);
	stop(a);
	stop(b);
	KUNIT_EXPECT_GT(test, atomic_read(&pick_calls) - before, 10);
	KUNIT_EXPECT_EQ(test, atomic_read(&pick_bad), 0);
}

/* Two spinners on CPU3 with idle siblings: nohz balancer kick reaches find_new_ilb. */
static void ilb_hook(struct kunit *test)
{
	struct task_struct *a = pinned(spin_fn, (void *)1500UL, 3, "avr-ilb-a");
	struct task_struct *b = pinned(spin_fn, (void *)1500UL, 3, "avr-ilb-b");

	KUNIT_ASSERT_FALSE(test, IS_ERR(a));
	KUNIT_ASSERT_FALSE(test, IS_ERR(b));
	wake_up_process(a);
	wake_up_process(b);
	msleep(1600);
	stop(a);
	stop(b);
	KUNIT_EXPECT_GT(test, atomic_read(&ilb_calls), 0);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(wakeup_preempt_hook), KUNIT_CASE(pick_hook), KUNIT_CASE(ilb_hook), {}
};
static struct kunit_suite suite = { .name = "avr-walt-hooks", .test_cases = cases };
kunit_test_suite(suite);

static int __init hooks_init(void)
{
	int ret;

	ret = register_trace_android_rvh_check_preempt_wakeup_fair(p_wake, NULL);
	ret = ret ?: register_trace_android_rvh_replace_next_task_fair(p_pick, NULL);
	ret = ret ?: register_trace_android_rvh_find_new_ilb(p_ilb, NULL);
	ret = ret ?: register_trace_android_vh_binder_set_priority(p_bset, NULL);
	ret = ret ?: register_trace_android_vh_binder_restore_priority(p_brestore, NULL);
	if (ret) {
		pr_err("AVR_WALT_HOOKS_REGISTER_FAIL %d\n", ret);
		return ret;
	}
	pr_info("AVR_WALT_HOOKS_REGISTERED\n");
	return 0;	/* the KUnit module notifier runs the suite after init */
}
module_init(hooks_init);
/* Restricted hooks cannot be unregistered: keep the module resident (like WALT). */
MODULE_LICENSE("GPL");
