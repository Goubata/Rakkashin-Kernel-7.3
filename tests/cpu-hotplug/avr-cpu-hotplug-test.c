// SPDX-License-Identifier: GPL-2.0-only
/* Full driver on native OF/platform/devres/cpuhp/thermal_of + real CPU hotplug
 * of QEMU CPUs 1-3. Only the temperature source is a private recorded value. */
#include <kunit/test.h>
#include <linux/cpu.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#undef module_driver
#define module_driver(driver, register, unregister)
#include "../../ports/qti-cpu-hotplug/cpu_hotplug.c"

static const struct of_device_id consumer_match[] = { { .compatible = "avr,cpu-hotplug-private" }, {} };

struct fixture {
	struct platform_device *consumer, *sensor;
	struct thermal_zone_device *zone;
	int temp;
	bool driver;
};

static int add_pdev(const char *name, const char *path, struct platform_device **out)
{
	struct device_node *np = of_find_node_by_path(path);
	struct platform_device *pdev;
	int ret;

	if (!np)
		return -ENOENT;
	pdev = platform_device_alloc(name, PLATFORM_DEVID_AUTO);
	if (!pdev) {
		of_node_put(np);
		return -ENOMEM;
	}
	device_set_node(&pdev->dev, of_fwnode_handle(np));
	ret = platform_device_add(pdev);
	if (ret) {
		platform_device_put(pdev);
		return ret;
	}
	*out = pdev;
	wait_for_device_probe();
	return 0;
}

static struct cpu_hot_ctx *ctx_of(struct fixture *f)
{
	return f->consumer ? platform_get_drvdata(f->consumer) : NULL;
}

static void settle(struct cpu_hot_ctx *ctx)
{
	int i, n;

	for (n = 0; ctx && n < 100; n++) {
		bool busy = false;

		for (i = 0; i < ctx->count; i++) {
			flush_work(&ctx->ent[i].reg_work);
			flush_work(&ctx->ent[i].exec_work);
			busy |= work_pending(&ctx->ent[i].exec_work) || work_pending(&ctx->ent[i].reg_work);
		}
		if (!busy)
			return;
	}
}

static struct cpu_hot_cdev *E(struct cpu_hot_ctx *ctx, int cpu)
{
	return cpu_hot_find(ctx, cpu);
}

static int add_consumer(struct fixture *f, const char *node)
{
	char path[64];
	int ret;

	snprintf(path, sizeof(path), "/avr-hotplug-fixture/%s", node);
	ret = add_pdev("avr-cpu-hotplug-private", path, &f->consumer);
	settle(ctx_of(f));
	return ret;
}

static int recorded_temp(struct thermal_zone_device *tz, int *temp)
{
	*temp = READ_ONCE(*(int *)thermal_zone_device_priv(tz));
	return 0;
}
static const struct thermal_zone_device_ops recorded_ops = { .get_temp = recorded_temp };

static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	static const u32 qemu_phandles[] = { 0x8004, 0x8003, 0x8002, 0x8001 };
	int cpu, ret;

	if (!f)
		return -ENOMEM;
	test->priv = f;
	for (cpu = 0; cpu < 4; cpu++) {
		struct device *d = get_cpu_device(cpu);

		if (!d || !d->of_node || d->of_node->phandle != qemu_phandles[cpu] || !cpu_online(cpu))
			return -EINVAL;
	}
	cpu_hot_driver.driver.name = "avr-cpu-hotplug-private";
	cpu_hot_driver.driver.of_match_table = consumer_match;
	ret = platform_driver_register(&cpu_hot_driver);
	if (!ret)
		f->driver = true;
	return ret;
}

static void teardown(struct kunit *test)
{
	struct fixture *f = test->priv;
	int cpu;

	if (f->consumer)
		platform_device_unregister(f->consumer);
	if (f->sensor)
		platform_device_unregister(f->sensor);
	if (f->driver)
		platform_driver_unregister(&cpu_hot_driver);
	for (cpu = 1; cpu < 4; cpu++) {
		KUNIT_EXPECT_TRUE(test, cpu_online(cpu));
		if (!cpu_online(cpu))
			add_cpu(cpu);
	}
}

static void probe_alias_and_of_node(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpu_hot_ctx *ctx;
	unsigned long v;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	KUNIT_EXPECT_EQ(test, ctx->count, 3);
	KUNIT_ASSERT_NOT_NULL(test, E(ctx, 1));
	KUNIT_ASSERT_NOT_NULL(test, E(ctx, 3));
	KUNIT_ASSERT_NOT_NULL(test, E(ctx, 2)->cdev);
	KUNIT_ASSERT_NOT_NULL(test, E(ctx, 3)->cdev);
	KUNIT_EXPECT_STREQ(test, E(ctx, 1)->cdev->type, "cpu-hotplug1");
	KUNIT_EXPECT_STREQ(test, E(ctx, 3)->cdev->type, "cpu-hotplug3");
	KUNIT_EXPECT_STREQ(test, E(ctx, 2)->cdev->np->name, "cpu2-hotplug");
	KUNIT_EXPECT_EQ(test, cpu_hot_get_max_state(E(ctx, 2)->cdev, &v), 0);
	KUNIT_EXPECT_EQ(test, v, 1UL);
	KUNIT_EXPECT_EQ(test, cpu_hot_set_cur_state(E(ctx, 2)->cdev, 2), -EINVAL);
}

static void mitigate_veto_restore(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpu_hot_ctx *ctx;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	KUNIT_ASSERT_EQ(test, cpu_hot_set_cur_state(E(ctx, 2)->cdev, 1), 0);
	settle(ctx);
	KUNIT_EXPECT_FALSE(test, cpu_online(2));
	KUNIT_EXPECT_NE(test, add_cpu(2), 0);
	KUNIT_EXPECT_FALSE(test, cpu_online(2));
	KUNIT_ASSERT_EQ(test, cpu_hot_set_cur_state(E(ctx, 2)->cdev, 0), 0);
	settle(ctx);
	KUNIT_EXPECT_TRUE(test, cpu_online(2));
}

static void teardown_restores_cpu(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpu_hot_ctx *ctx;
	int i;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	for (i = 0; i < 3; i++) {
		ctx = ctx_of(f);
		KUNIT_ASSERT_NOT_NULL(test, ctx);
		KUNIT_ASSERT_EQ(test, cpu_hot_set_cur_state(E(ctx, 3)->cdev, 1), 0);
		settle(ctx);
		KUNIT_EXPECT_FALSE(test, cpu_online(3));
		device_release_driver(&f->consumer->dev);
		KUNIT_EXPECT_TRUE(test, cpu_online(3));
		KUNIT_ASSERT_EQ(test, device_attach(&f->consumer->dev), 1);
		settle(ctx_of(f));
	}
}

static void offline_at_probe_registers_later(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpu_hot_ctx *ctx;

	KUNIT_ASSERT_EQ(test, remove_cpu(3), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	KUNIT_EXPECT_NULL(test, E(ctx, 3)->cdev);
	KUNIT_ASSERT_EQ(test, add_cpu(3), 0);
	settle(ctx);
	KUNIT_EXPECT_NOT_NULL(test, E(ctx, 3)->cdev);
}

static void rapid_toggle_converges(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpu_hot_ctx *ctx;
	int i;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	for (i = 0; i < 51; i++)	/* ends with 1 */
		cpu_hot_set_cur_state(E(ctx, 1)->cdev, (i + 1) & 1);
	settle(ctx);
	KUNIT_EXPECT_FALSE(test, cpu_online(1));
	for (i = 0; i < 51; i++)	/* ends with 0 */
		cpu_hot_set_cur_state(E(ctx, 1)->cdev, i & 1);
	settle(ctx);
	KUNIT_EXPECT_TRUE(test, cpu_online(1));
}

static void user_offline_is_not_onlined(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpu_hot_ctx *ctx;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	KUNIT_ASSERT_EQ(test, remove_cpu(1), 0);
	cpu_hot_set_cur_state(E(ctx, 1)->cdev, 1);
	settle(ctx);
	cpu_hot_set_cur_state(E(ctx, 1)->cdev, 0);
	settle(ctx);
	KUNIT_EXPECT_FALSE(test, cpu_online(1));
	KUNIT_EXPECT_EQ(test, add_cpu(1), 0);
}

static void dt_cooling_map_isolates_cpu(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpu_hot_ctx *ctx;
	unsigned long v;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	f->temp = 30000;
	KUNIT_ASSERT_EQ(test, add_pdev("avr-hp-temp", "/avr-hp-sensor", &f->sensor), 0);
	f->zone = devm_thermal_of_zone_register(&f->sensor->dev, 0, &f->temp, &recorded_ops);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->zone));
	WRITE_ONCE(f->temp, 60000);
	thermal_zone_device_update(f->zone, THERMAL_EVENT_UNSPECIFIED);
	settle(ctx);
	KUNIT_EXPECT_EQ(test, cpu_hot_get_cur_state(E(ctx, 2)->cdev, &v), 0);
	KUNIT_EXPECT_EQ(test, v, 1UL);
	KUNIT_EXPECT_FALSE(test, cpu_online(2));
	KUNIT_EXPECT_TRUE(test, cpu_online(1));
	WRITE_ONCE(f->temp, 20000);
	thermal_zone_device_update(f->zone, THERMAL_EVENT_UNSPECIFIED);
	settle(ctx);
	KUNIT_EXPECT_TRUE(test, cpu_online(2));
}

static void no_valid_cpu_rejected(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "none"), 0);
	KUNIT_EXPECT_PTR_EQ(test, f->consumer->dev.driver, NULL);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(probe_alias_and_of_node), KUNIT_CASE(mitigate_veto_restore),
	KUNIT_CASE(teardown_restores_cpu), KUNIT_CASE(offline_at_probe_registers_later),
	KUNIT_CASE(rapid_toggle_converges), KUNIT_CASE(user_offline_is_not_onlined),
	KUNIT_CASE(dt_cooling_map_isolates_cpu), KUNIT_CASE(no_valid_cpu_rejected), {}
};
static struct kunit_suite suite = {
	.name = "avr-cpu-hotplug", .init = setup, .exit = teardown, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
