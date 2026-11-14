// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include "../../ports/gunyah-rm/avr-rm-notifiers.h"

struct fixture;
struct observer {
	struct notifier_block nb;
	struct fixture *fixture;
	unsigned int index;
};
struct fixture {
	struct avr_rm_notifiers state;
	struct observer observers[2];
	struct blocking_notifier_head heads[2];
	unsigned int fail_stage, events[16], event_count, dispatched[2];
	bool block_first;
	struct completion entered, release, cleanup_started, cleanup_done;
};
static void record(struct fixture *f, unsigned int event)
{
	if (f->event_count < ARRAY_SIZE(f->events))
		f->events[f->event_count++] = event;
}
static int observed(struct notifier_block *nb, unsigned long event, void *arg)
{
	struct observer *o = container_of(nb, struct observer, nb);
	struct fixture *f = o->fixture;

	f->dispatched[o->index]++;
	if (!o->index && f->block_first) {
		complete(&f->entered);
		wait_for_completion_timeout(&f->release, msecs_to_jiffies(2000));
	}
	return NOTIFY_OK;
}
static int register_observer(struct notifier_block *nb)
{
	struct observer *o = container_of(nb, struct observer, nb);
	struct fixture *f = o->fixture;

	record(f, o->index * 2 + 1);
	if (f->fail_stage == o->index + 1)
		return o->index ? -EIO : -ENOMEM;
	return blocking_notifier_chain_register(&f->heads[o->index], nb);
}
static int unregister_observer(struct notifier_block *nb)
{
	struct observer *o = container_of(nb, struct observer, nb);

	record(o->fixture, o->index * 2 + 2);
	return blocking_notifier_chain_unregister(&o->fixture->heads[o->index], nb);
}
static const struct avr_rm_notifier_ops ops = {
	.register_first = register_observer, .unregister_first = unregister_observer,
	.register_second = register_observer, .unregister_second = unregister_observer,
};
static int init(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	unsigned int i;

	if (!f)
		return -ENOMEM;
	test->priv = f;
	for (i = 0; i < 2; i++) {
		BLOCKING_INIT_NOTIFIER_HEAD(&f->heads[i]);
		f->observers[i].fixture = f;
		f->observers[i].index = i;
		f->observers[i].nb.notifier_call = observed;
	}
	init_completion(&f->entered);
	init_completion(&f->release);
	init_completion(&f->cleanup_started);
	init_completion(&f->cleanup_done);
	f->state.ops = &ops;
	f->state.first = &f->observers[0].nb;
	f->state.second = &f->observers[1].nb;
	return 0;
}
static void exit_test(struct kunit *test)
{
	struct fixture *f = test->priv;

	avr_rm_notifiers_unregister(&f->state);
}
static void dispatch(struct fixture *f)
{
	blocking_notifier_call_chain(&f->heads[0], 0, NULL);
	blocking_notifier_call_chain(&f->heads[1], 0, NULL);
}
static void successful_pair_and_reverse_cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, avr_rm_notifiers_register(&f->state), 0);
	dispatch(f);
	KUNIT_EXPECT_EQ(test, f->dispatched[0], 1U);
	KUNIT_EXPECT_EQ(test, f->dispatched[1], 1U);
	avr_rm_notifiers_unregister(&f->state);
	KUNIT_ASSERT_EQ(test, f->event_count, 4U);
	KUNIT_EXPECT_EQ(test, f->events[0], 1U);
	KUNIT_EXPECT_EQ(test, f->events[1], 3U);
	KUNIT_EXPECT_EQ(test, f->events[2], 4U);
	KUNIT_EXPECT_EQ(test, f->events[3], 2U);
	dispatch(f);
	KUNIT_EXPECT_EQ(test, f->dispatched[0], 1U);
	KUNIT_EXPECT_EQ(test, f->dispatched[1], 1U);
}
static void first_registration_failure(struct kunit *test)
{
	struct fixture *f = test->priv;

	f->fail_stage = 1;
	KUNIT_EXPECT_EQ(test, avr_rm_notifiers_register(&f->state), -ENOMEM);
	KUNIT_EXPECT_FALSE(test, f->state.first_registered);
	KUNIT_EXPECT_FALSE(test, f->state.second_registered);
	dispatch(f);
	KUNIT_EXPECT_EQ(test, f->dispatched[0] + f->dispatched[1], 0U);
	KUNIT_EXPECT_EQ(test, f->event_count, 1U);
}
static void second_failure_unwinds_first(struct kunit *test)
{
	struct fixture *f = test->priv;

	f->fail_stage = 2;
	KUNIT_EXPECT_EQ(test, avr_rm_notifiers_register(&f->state), -EIO);
	KUNIT_EXPECT_FALSE(test, f->state.first_registered);
	KUNIT_EXPECT_FALSE(test, f->state.second_registered);
	KUNIT_ASSERT_EQ(test, f->event_count, 3U);
	KUNIT_EXPECT_EQ(test, f->events[2], 2U);
	dispatch(f);
	KUNIT_EXPECT_EQ(test, f->dispatched[0] + f->dispatched[1], 0U);
}
static void reject_duplicate_without_changes(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, avr_rm_notifiers_register(&f->state), 0);
	KUNIT_EXPECT_EQ(test, avr_rm_notifiers_register(&f->state), -EALREADY);
	KUNIT_EXPECT_EQ(test, f->event_count, 2U);
}
static void optional_second_handler(struct kunit *test)
{
	struct fixture *f = test->priv;

	f->state.second = NULL;
	KUNIT_ASSERT_EQ(test, avr_rm_notifiers_register(&f->state), 0);
	dispatch(f);
	KUNIT_EXPECT_EQ(test, f->dispatched[0], 1U);
	KUNIT_EXPECT_EQ(test, f->dispatched[1], 0U);
	avr_rm_notifiers_unregister(&f->state);
	KUNIT_EXPECT_EQ(test, f->event_count, 2U);
}
static void cleanup_is_idempotent(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, avr_rm_notifiers_register(&f->state), 0);
	avr_rm_notifiers_unregister(&f->state);
	avr_rm_notifiers_unregister(&f->state);
	KUNIT_EXPECT_EQ(test, f->event_count, 4U);
}
static void retry_after_rollback(struct kunit *test)
{
	struct fixture *f = test->priv;

	f->fail_stage = 2;
	KUNIT_ASSERT_EQ(test, avr_rm_notifiers_register(&f->state), -EIO);
	f->fail_stage = 0;
	KUNIT_ASSERT_EQ(test, avr_rm_notifiers_register(&f->state), 0);
	dispatch(f);
	KUNIT_EXPECT_EQ(test, f->dispatched[0], 1U);
	KUNIT_EXPECT_EQ(test, f->dispatched[1], 1U);
}
static int dispatch_thread(void *arg)
{
	struct fixture *f = arg;

	blocking_notifier_call_chain(&f->heads[0], 0, NULL);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(msecs_to_jiffies(10));
	return 0;
}
static int cleanup_thread(void *arg)
{
	struct fixture *f = arg;

	complete(&f->cleanup_started);
	avr_rm_notifiers_unregister(&f->state);
	complete(&f->cleanup_done);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(msecs_to_jiffies(10));
	return 0;
}
static void cleanup_waits_for_inflight_callback(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct task_struct *caller, *cleaner;
	unsigned long entered;

	KUNIT_ASSERT_EQ(test, avr_rm_notifiers_register(&f->state), 0);
	f->block_first = true;
	caller = kthread_run(dispatch_thread, f, "avr-rm-notify");
	KUNIT_ASSERT_FALSE(test, IS_ERR(caller));
	entered = wait_for_completion_timeout(&f->entered, msecs_to_jiffies(1000));
	if (!entered) {
		complete(&f->release);
		kthread_stop(caller);
		KUNIT_FAIL(test, "Callback did not enter");
		return;
	}
	cleaner = kthread_run(cleanup_thread, f, "avr-rm-cleanup");
	if (IS_ERR(cleaner)) {
		complete(&f->release);
		kthread_stop(caller);
		KUNIT_FAIL(test, "Cleanup thread did not start");
		return;
	}
	KUNIT_EXPECT_NE(test, wait_for_completion_timeout(&f->cleanup_started,
		msecs_to_jiffies(1000)), 0UL);
	msleep(10);
	KUNIT_EXPECT_FALSE(test, completion_done(&f->cleanup_done));
	complete(&f->release);
	KUNIT_EXPECT_NE(test, wait_for_completion_timeout(&f->cleanup_done,
		msecs_to_jiffies(1000)), 0UL);
	kthread_stop(caller);
	kthread_stop(cleaner);
	KUNIT_EXPECT_FALSE(test, f->state.first_registered);
	KUNIT_EXPECT_FALSE(test, f->state.second_registered);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(successful_pair_and_reverse_cleanup), KUNIT_CASE(first_registration_failure),
	KUNIT_CASE(second_failure_unwinds_first), KUNIT_CASE(reject_duplicate_without_changes),
	KUNIT_CASE(optional_second_handler), KUNIT_CASE(cleanup_is_idempotent),
	KUNIT_CASE(retry_after_rollback), KUNIT_CASE(cleanup_waits_for_inflight_callback), {}
};
static struct kunit_suite suite = {
	.name = "avr-rm-notifier-lifetime", .init = init, .exit = exit_test, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Actual RM notifier-pair registration/rollback lifetime tests");
