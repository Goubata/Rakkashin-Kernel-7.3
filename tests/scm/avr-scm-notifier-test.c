// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/delay.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include "qcom_pas_retry.h"

enum decision { PASS, HANDLED, DENIED, SLEEP_HANDLED };
struct observer {
	struct notifier_block nb;
	enum decision decision;
	unsigned int calls;
	bool registered;
	phys_addr_t address;
	size_t size;
	u64 observed_src;
	unsigned int dest_count;
};
struct context { struct observer first, second; };

static int notified(struct notifier_block *nb, unsigned long action, void *arg)
{
	struct observer *o = container_of(nb, struct observer, nb);
	struct qcom_scm_assign_mem_notifier_data *data = arg;

	o->calls++;
	o->address = data->mem_addr;
	o->size = data->mem_sz;
	o->observed_src = *data->srcvm;
	o->dest_count = data->dest_cnt;
	if (o->decision == DENIED)
		return notifier_from_errno(-EACCES);
	if (o->decision == SLEEP_HANDLED)
		msleep(1);
	if (o->decision == HANDLED || o->decision == SLEEP_HANDLED) {
		/* A test subscriber models the documented handled outcome. No
		 * firmware, hypercall or production replacement backend is used. */
		*data->srcvm = BIT_ULL(data->newvm[0].vmid);
		return NOTIFY_STOP;
	}
	return NOTIFY_OK;
}
static int init(struct kunit *test)
{
	struct context *ctx;
	int ret;

	/* This suite must only run in the diskless guest without SCM hardware. */
	if (qcom_scm_is_available())
		return -EBUSY;
	ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;
	ctx->first.nb.notifier_call = notified;
	ctx->first.nb.priority = 100;
	ctx->second.nb.notifier_call = notified;
	ret = qcom_scm_assign_mem_notifier_register(&ctx->first.nb);
	if (ret)
		return ret;
	ctx->first.registered = true;
	return 0;
}
static void exit_test(struct kunit *test)
{
	struct context *ctx = test->priv;

	if (ctx->second.registered)
		qcom_scm_assign_mem_notifier_unregister(&ctx->second.nb);
	if (ctx->first.registered)
		qcom_scm_assign_mem_notifier_unregister(&ctx->first.nb);
}
static int assign(u64 *mask)
{
	const struct qcom_scm_vmperm dest = { .vmid = 60, .perm = 3 };

	return qcom_scm_assign_mem(0x1000, PAGE_SIZE, mask, &dest, 1);
}
static void no_handler(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);

	KUNIT_ASSERT_EQ(test, qcom_scm_assign_mem_notifier_unregister(&ctx->first.nb), 0);
	ctx->first.registered = false;
	KUNIT_EXPECT_EQ(test, assign(&mask), -EPROBE_DEFER);
	KUNIT_EXPECT_EQ(test, mask, BIT_ULL(3));
}
static void pass_to_real_backend(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);

	KUNIT_EXPECT_EQ(test, assign(&mask), -EPROBE_DEFER);
	KUNIT_EXPECT_EQ(test, ctx->first.calls, 1U);
	KUNIT_EXPECT_EQ(test, mask, BIT_ULL(3));
}
static void handled_metadata(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);

	ctx->first.decision = HANDLED;
	KUNIT_EXPECT_EQ(test, assign(&mask), 0);
	KUNIT_EXPECT_EQ(test, mask, BIT_ULL(60));
	KUNIT_EXPECT_EQ(test, ctx->first.address, (phys_addr_t)0x1000);
	KUNIT_EXPECT_EQ(test, ctx->first.size, (size_t)PAGE_SIZE);
	KUNIT_EXPECT_EQ(test, ctx->first.observed_src, BIT_ULL(3));
	KUNIT_EXPECT_EQ(test, ctx->first.dest_count, 1U);
}
static void propagate_denial(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);

	ctx->first.decision = DENIED;
	KUNIT_EXPECT_EQ(test, assign(&mask), -EACCES);
	KUNIT_EXPECT_EQ(test, mask, BIT_ULL(3));
}
static void reject_invalid_before_notifier(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);
	struct qcom_scm_vmperm dest = { .vmid = 60, .perm = 3 };

	ctx->first.decision = HANDLED;
	KUNIT_EXPECT_EQ(test, qcom_scm_assign_mem(0x1000, PAGE_SIZE, NULL, &dest, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_assign_mem(0x1000, PAGE_SIZE, &mask, NULL, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_assign_mem(0x1000, PAGE_SIZE, &mask, &dest, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_assign_mem(0x1000, PAGE_SIZE, &mask, &dest, 65), -EINVAL);
	dest.vmid = 64;
	KUNIT_EXPECT_EQ(test, qcom_scm_assign_mem(0x1000, PAGE_SIZE, &mask, &dest, 1), -EINVAL);
	dest.vmid = -1;
	KUNIT_EXPECT_EQ(test, qcom_scm_assign_mem(0x1000, PAGE_SIZE, &mask, &dest, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, ctx->first.calls, 0U);
	KUNIT_EXPECT_EQ(test, mask, BIT_ULL(3));
}
static void stop_prevents_second_handler(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);

	KUNIT_ASSERT_EQ(test, qcom_scm_assign_mem_notifier_register(&ctx->second.nb), 0);
	ctx->second.registered = true;
	ctx->first.decision = HANDLED;
	KUNIT_EXPECT_EQ(test, assign(&mask), 0);
	KUNIT_EXPECT_EQ(test, ctx->second.calls, 0U);
}
static void unregister_suppresses_callback(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);

	ctx->first.decision = HANDLED;
	KUNIT_ASSERT_EQ(test, assign(&mask), 0);
	KUNIT_ASSERT_EQ(test, qcom_scm_assign_mem_notifier_unregister(&ctx->first.nb), 0);
	ctx->first.registered = false;
	KUNIT_EXPECT_EQ(test, assign(&mask), -EPROBE_DEFER);
	KUNIT_EXPECT_EQ(test, ctx->first.calls, 1U);
}
static void callback_may_sleep(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 mask = BIT_ULL(3);

	ctx->first.decision = SLEEP_HANDLED;
	KUNIT_EXPECT_EQ(test, assign(&mask), 0);
	KUNIT_EXPECT_EQ(test, mask, BIT_ULL(60));
}
static struct kunit *retry_test;
static unsigned int shutdown_calls, delay_calls, observed_delay;
static int retry_results[2];
static int shutdown_for_test(u32 id)
{
	KUNIT_EXPECT_EQ(retry_test, id, (u32)13);
	if (shutdown_calls >= 2) {
		KUNIT_FAIL(retry_test, "More than one retry");
		return -EIO;
	}
	return retry_results[shutdown_calls++];
}
static void delay_for_test(unsigned int delay)
{
	delay_calls++;
	observed_delay = delay;
}
static void setup_retry(struct kunit *test, int first, int second)
{
	retry_test = test;
	shutdown_calls = delay_calls = observed_delay = 0;
	retry_results[0] = first;
	retry_results[1] = second;
}
static void pas_initial_success(struct kunit *test)
{
	setup_retry(test, 0, -EIO);
	KUNIT_EXPECT_EQ(test, qcom_pas_retry(13, 5000, shutdown_for_test, delay_for_test), 0);
	KUNIT_EXPECT_EQ(test, shutdown_calls, 1U);
	KUNIT_EXPECT_EQ(test, delay_calls, 0U);
}
static void pas_disabled_retry_preserves_error(struct kunit *test)
{
	setup_retry(test, -EACCES, 0);
	KUNIT_EXPECT_EQ(test, qcom_pas_retry(13, 0, shutdown_for_test, delay_for_test), -EACCES);
	KUNIT_EXPECT_EQ(test, shutdown_calls, 1U);
	KUNIT_EXPECT_EQ(test, delay_calls, 0U);
}
static void pas_second_attempt_success(struct kunit *test)
{
	setup_retry(test, -EBUSY, 0);
	KUNIT_EXPECT_EQ(test, qcom_pas_retry(13, 5000, shutdown_for_test, delay_for_test), 0);
	KUNIT_EXPECT_EQ(test, shutdown_calls, 2U);
	KUNIT_EXPECT_EQ(test, delay_calls, 1U);
	KUNIT_EXPECT_EQ(test, observed_delay, 5000U);
}
static void pas_final_error(struct kunit *test)
{
	setup_retry(test, -EBUSY, -EIO);
	KUNIT_EXPECT_EQ(test, qcom_pas_retry(13, 5000, shutdown_for_test, delay_for_test), -EIO);
	KUNIT_EXPECT_EQ(test, shutdown_calls, 2U);
	KUNIT_EXPECT_EQ(test, delay_calls, 1U);
}
static void pas_without_hardware_defers(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, qcom_scm_pas_shutdown_retry(13), -EPROBE_DEFER);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(no_handler), KUNIT_CASE(pass_to_real_backend), KUNIT_CASE(handled_metadata),
	KUNIT_CASE(propagate_denial), KUNIT_CASE(reject_invalid_before_notifier),
	KUNIT_CASE(stop_prevents_second_handler), KUNIT_CASE(unregister_suppresses_callback),
	KUNIT_CASE(callback_may_sleep),
	KUNIT_CASE(pas_initial_success), KUNIT_CASE(pas_disabled_retry_preserves_error),
	KUNIT_CASE(pas_second_attempt_success), KUNIT_CASE(pas_final_error),
	KUNIT_CASE(pas_without_hardware_defers), {}
};
static struct kunit_suite suite = {
	.name = "avr-scm-assign-notifier", .init = init, .exit = exit_test, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Real SCM assign-memory notification contract tests without firmware");
