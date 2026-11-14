// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/notifier.h>
#include <linux/remoteproc.h>
#include <linux/remoteproc/qcom_rproc.h>
#include "qcom_common.h"

struct observer {
	struct notifier_block nb;
	void *cookie;
	unsigned int count;
	unsigned long events[8];
	bool crashed[8];
	const char *name;
	bool may_sleep;
};
struct context {
	struct device *dev;
	struct rproc *rproc;
	struct qcom_rproc_ssr ssr;
	struct observer primary, other;
};
static int notified(struct notifier_block *nb, unsigned long event, void *arg)
{
	struct observer *o = container_of(nb, struct observer, nb);
	struct qcom_ssr_notify_data *data = arg;
	unsigned int slot = o->count++;

	if (o->may_sleep)
		msleep(1);
	if (slot < ARRAY_SIZE(o->events)) {
		o->events[slot] = event;
		o->crashed[slot] = data->crashed;
	}
	o->name = data->name;
	return NOTIFY_OK;
}
static int init(struct kunit *test)
{
	static const struct rproc_ops ops;
	struct context *ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;
	test->priv = ctx;
	ctx->dev = root_device_register("avr-rproc-kunit");
	if (IS_ERR(ctx->dev))
		return PTR_ERR(ctx->dev);
	ctx->rproc = rproc_alloc(ctx->dev, "avr-kunit", &ops, "unused-kunit-fw", 0);
	if (!ctx->rproc) {
		root_device_unregister(ctx->dev);
		return -ENOMEM;
	}
	ctx->primary.nb.notifier_call = notified;
	ctx->other.nb.notifier_call = notified;
	ctx->primary.cookie = qcom_register_ssr_notifier("avr-kunit-ssr", &ctx->primary.nb);
	if (IS_ERR(ctx->primary.cookie))
		goto fail;
	ctx->other.cookie = qcom_register_ssr_notifier("avr-kunit-other", &ctx->other.nb);
	if (IS_ERR(ctx->other.cookie)) {
		qcom_unregister_ssr_notifier(ctx->primary.cookie, &ctx->primary.nb);
		goto fail;
	}
	qcom_add_ssr_subdev(ctx->rproc, &ctx->ssr, "avr-kunit-ssr");
	if (!ctx->ssr.info) {
		qcom_unregister_ssr_notifier(ctx->other.cookie, &ctx->other.nb);
		qcom_unregister_ssr_notifier(ctx->primary.cookie, &ctx->primary.nb);
		goto fail;
	}
	return 0;
fail:
	rproc_free(ctx->rproc);
	root_device_unregister(ctx->dev);
	return -ENOMEM;
}
static void exit_test(struct kunit *test)
{
	struct context *ctx = test->priv;

	qcom_remove_ssr_subdev(ctx->rproc, &ctx->ssr);
	if (ctx->primary.cookie)
		qcom_unregister_ssr_notifier(ctx->primary.cookie, &ctx->primary.nb);
	qcom_unregister_ssr_notifier(ctx->other.cookie, &ctx->other.nb);
	rproc_free(ctx->rproc);
	root_device_unregister(ctx->dev);
}
static void cycle(struct kunit *test, bool crashed)
{
	struct context *ctx = test->priv;
	struct rproc_subdev *sub = &ctx->ssr.subdev;

	KUNIT_EXPECT_EQ(test, sub->prepare(sub), 0);
	KUNIT_EXPECT_EQ(test, sub->start(sub), 0);
	sub->stop(sub, crashed);
	sub->unprepare(sub);
}
static void lifecycle(struct kunit *test)
{
	struct context *ctx = test->priv;
	unsigned int i;

	cycle(test, false);
	KUNIT_EXPECT_EQ(test, ctx->primary.count, 4U);
	for (i = 0; i < 4; i++) {
		KUNIT_EXPECT_EQ(test, ctx->primary.events[i], (unsigned long)i);
		KUNIT_EXPECT_FALSE(test, ctx->primary.crashed[i]);
	}
	KUNIT_EXPECT_STREQ(test, ctx->primary.name, "avr-kunit-ssr");
}
static void crash_event(struct kunit *test)
{
	struct context *ctx = test->priv;

	cycle(test, true);
	KUNIT_EXPECT_EQ(test, ctx->primary.count, 4U);
	KUNIT_EXPECT_TRUE(test, ctx->primary.crashed[QCOM_SSR_BEFORE_SHUTDOWN]);
	KUNIT_EXPECT_FALSE(test, ctx->primary.crashed[QCOM_SSR_AFTER_SHUTDOWN]);
}
static void isolated_subsystems(struct kunit *test)
{
	struct context *ctx = test->priv;

	cycle(test, false);
	KUNIT_EXPECT_EQ(test, ctx->other.count, 0U);
	KUNIT_EXPECT_EQ(test, ctx->primary.count, 4U);
}
static void unsubscribe(struct kunit *test)
{
	struct context *ctx = test->priv;

	cycle(test, false);
	KUNIT_ASSERT_EQ(test, qcom_unregister_ssr_notifier(ctx->primary.cookie, &ctx->primary.nb), 0);
	ctx->primary.cookie = NULL;
	cycle(test, true);
	KUNIT_EXPECT_EQ(test, ctx->primary.count, 4U);
}
static void sleepable(struct kunit *test)
{
	struct context *ctx = test->priv;

	ctx->primary.may_sleep = true;
	cycle(test, false);
	KUNIT_EXPECT_EQ(test, ctx->primary.count, 4U);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(lifecycle), KUNIT_CASE(crash_event), KUNIT_CASE(isolated_subsystems),
	KUNIT_CASE(unsubscribe), KUNIT_CASE(sleepable), {}
};
static struct kunit_suite suite = {
	.name = "avr-remoteproc-ssr", .init = init, .exit = exit_test, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AVR real remoteproc SSR notification lifecycle tests");
