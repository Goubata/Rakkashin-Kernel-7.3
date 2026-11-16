// SPDX-License-Identifier: GPL-2.0-only
/* Full driver on native OF/platform/devres/cpufreq core/freq-QoS/cpuhp/thermal.
 * Only the cpufreq driver below is private: it publishes the AVR phone tables
 * read from 6.12 sysfs (policy0 24 levels, policy6 21 + 4 boost levels). */
#include <kunit/test.h>
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#undef pr_fmt
#undef module_driver
#define module_driver(driver, register, unregister)
#include "../../ports/qti-cpufreq-cdev/qti_cpufreq_cdev.c"

static const unsigned int phone_policy0[] = {
	384000, 556800, 710400, 806400, 902400, 1036800, 1152000, 1267200, 1401600, 1555200,
	1651200, 1747200, 1900800, 2016000, 2188800, 2265600, 2361600, 2476800, 2611200,
	2707200, 2841600, 3033600, 3129600, 3321600,
};
static const unsigned int phone_policy6[] = {
	384000, 556800, 806400, 921600, 1017600, 1152000, 1248000, 1382400, 1497600, 1651200,
	1862400, 1996800, 2112000, 2227200, 2476800, 2688000, 2841600, 2995200, 3091200,
	3244800, 3436800,
};
static const unsigned int phone_policy6_boost[] = { 3532800, 3648000, 3724800, 3801600 };

static struct cpufreq_frequency_table t0[32], t1[32], tbad[8];
static bool use_bad_table;
static unsigned int cur_khz[NR_CPUS];

static void fill_tables(void)
{
	int i, n = 0;

	for (i = 0; i < ARRAY_SIZE(phone_policy0); i++)
		t0[i] = (struct cpufreq_frequency_table){ .frequency = phone_policy0[i] };
	t0[i].frequency = CPUFREQ_TABLE_END;
	for (i = 0; i < ARRAY_SIZE(phone_policy6); i++)
		t1[n++] = (struct cpufreq_frequency_table){ .frequency = phone_policy6[i] };
	for (i = 0; i < ARRAY_SIZE(phone_policy6_boost); i++)
		t1[n++] = (struct cpufreq_frequency_table){ .flags = CPUFREQ_BOOST_FREQ,
							     .frequency = phone_policy6_boost[i] };
	t1[n].frequency = CPUFREQ_TABLE_END;
	/* Unsorted, an invalid hole and a duplicate. */
	tbad[0].frequency = 1000000; tbad[1].frequency = CPUFREQ_ENTRY_INVALID;
	tbad[2].frequency = 300000;  tbad[3].frequency = 2000000;
	tbad[4].frequency = 1000000; tbad[5].frequency = CPUFREQ_TABLE_END;
}

static int fake_init(struct cpufreq_policy *policy)
{
	int first = policy->cpu < 2 ? 0 : 2;

	cpumask_set_cpu(first, policy->cpus);
	cpumask_set_cpu(first + 1, policy->cpus);
	policy->freq_table = first ? t1 : (use_bad_table ? tbad : t0);
	policy->cpuinfo.transition_latency = 10000;
	cur_khz[policy->cpu] = policy->freq_table[0].frequency;
	return 0;
}

static int fake_target_index(struct cpufreq_policy *policy, unsigned int index)
{
	int cpu;

	for_each_cpu(cpu, policy->cpus)
		cur_khz[cpu] = policy->freq_table[index].frequency;
	return 0;
}

static unsigned int fake_get(unsigned int cpu) { return cur_khz[cpu]; }

static struct cpufreq_driver fake_cpufreq = {
	.name = "avr-fake-cpufreq",
	.init = fake_init,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = fake_target_index,
	.get = fake_get,
};

static const struct of_device_id consumer_match[] = { { .compatible = "avr,cpufreq-cdev-private" }, {} };

struct fixture {
	struct platform_device *consumer;
	bool cpufreq, consumer_driver;
};

static int add_consumer(struct fixture *f, const char *node)
{
	char path[96];
	struct device_node *np;
	struct platform_device *pdev;
	int ret;

	snprintf(path, sizeof(path), "/avr-cpufreq-fixture/%s", node);
	np = of_find_node_by_path(path);
	if (!np)
		return -ENOENT;
	pdev = platform_device_alloc("avr-cpufreq-cdev-private", PLATFORM_DEVID_AUTO);
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
	f->consumer = pdev;
	wait_for_device_probe();
	return 0;
}

static struct cpufreq_cdev_ctx *ctx_of(struct fixture *f)
{
	return f->consumer ? platform_get_drvdata(f->consumer) : NULL;
}

/* Child order is DT-merge dependent; select by cluster name like the HAL does. */
static struct cpufreq_cdev_device *C(struct cpufreq_cdev_ctx *ctx, int n)
{
	char name[THERMAL_NAME_LENGTH];
	int i;

	snprintf(name, sizeof(name), "cpu-cluster%d", n);
	for (i = 0; i < ctx->count; i++)
		if (!strcmp(ctx->cl[i].cdev_name, name))
			return &ctx->cl[i];
	return C(ctx, 0);
}

static void settle(struct cpufreq_cdev_ctx *ctx)
{
	int i;

	for (i = 0; ctx && i < ctx->count; i++)
		flush_work(&ctx->cl[i].reg_work);
}

static int start_cpufreq(struct fixture *f)
{
	int ret = cpufreq_register_driver(&fake_cpufreq);

	if (!ret)
		f->cpufreq = true;
	settle(ctx_of(f));
	return ret;
}

static void stop_cpufreq(struct fixture *f)
{
	if (f->cpufreq)
		cpufreq_unregister_driver(&fake_cpufreq);
	f->cpufreq = false;
}

static s32 qos_max(int cpu)
{
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	s32 v;

	if (!policy)
		return -1;
	v = READ_ONCE(policy->constraints.max_freq.target_value);
	cpufreq_cpu_put(policy);
	return v;
}

static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	static const u32 qemu_phandles[] = { 0x8004, 0x8003, 0x8002, 0x8001 };
	int cpu, ret;

	if (!f)
		return -ENOMEM;
	test->priv = f;
	/* Fixture DT uses QEMU virt -smp4 CPU phandles; detect drift instead of passing vacuously. */
	for (cpu = 0; cpu < 4; cpu++) {
		struct device *d = get_cpu_device(cpu);

		if (!d || !d->of_node || d->of_node->phandle != qemu_phandles[cpu])
			return -EINVAL;
	}
	fill_tables();
	use_bad_table = false;
	cpufreq_cdev_driver.driver.name = "avr-cpufreq-cdev-private";
	cpufreq_cdev_driver.driver.of_match_table = consumer_match;
	ret = platform_driver_register(&cpufreq_cdev_driver);
	if (ret)
		return ret;
	f->consumer_driver = true;
	return 0;
}

static void teardown(struct kunit *test)
{
	struct fixture *f = test->priv;

	if (f->consumer)
		platform_device_unregister(f->consumer);
	f->consumer = NULL;
	if (f->consumer_driver)
		platform_driver_unregister(&cpufreq_cdev_driver);
	stop_cpufreq(f);
}

static void expect_table(struct kunit *test, struct cpufreq_cdev_device *c,
			 const unsigned int *asc, int n, const unsigned int *boost, int nb)
{
	int i;

	KUNIT_ASSERT_EQ(test, c->max_state, (unsigned long)(n + nb - 1));
	for (i = 0; i < nb; i++)
		KUNIT_EXPECT_EQ(test, c->freq_table[i], boost[nb - 1 - i]);
	for (i = 0; i < n; i++)
		KUNIT_EXPECT_EQ(test, c->freq_table[nb + i], asc[n - 1 - i]);
}

static void probe_saved_tables(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpufreq_cdev_ctx *ctx;

	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	settle(ctx);
	KUNIT_ASSERT_EQ(test, ctx->count, 2);
	KUNIT_ASSERT_NOT_NULL(test, C(ctx, 0)->cdev);
	KUNIT_ASSERT_NOT_NULL(test, C(ctx, 1)->cdev);
	KUNIT_EXPECT_STREQ(test, C(ctx, 0)->cdev->type, "cpu-cluster0");
	KUNIT_EXPECT_STREQ(test, C(ctx, 1)->cdev->type, "cpu-cluster1");
	KUNIT_EXPECT_EQ(test, C(ctx, 0)->cpu, 0);
	KUNIT_EXPECT_STRNEQ(test, C(ctx, 0)->cdev_name, C(ctx, 1)->cdev_name);
	KUNIT_EXPECT_EQ(test, C(ctx, 1)->cpu, 2);
	/* Phone: cpu-cluster0 max_state 23, cpu-cluster1 max_state 24. */
	expect_table(test, C(ctx, 0), phone_policy0, ARRAY_SIZE(phone_policy0), NULL, 0);
	expect_table(test, C(ctx, 1), phone_policy6, ARRAY_SIZE(phone_policy6),
		     phone_policy6_boost, ARRAY_SIZE(phone_policy6_boost));
}

static void limits_and_release(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpufreq_cdev_ctx *ctx;
	s32 base0, base2;
	unsigned long state;

	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	base0 = qos_max(0);
	base2 = qos_max(2);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	settle(ctx);
	KUNIT_ASSERT_NOT_NULL(test, C(ctx, 0)->cdev);
	KUNIT_EXPECT_EQ(test, cpufreq_cdev_set_state(C(ctx, 0)->cdev, 5), 0);
	KUNIT_EXPECT_EQ(test, qos_max(0), 2611200);
	KUNIT_EXPECT_EQ(test, qos_max(1), 2611200);
	KUNIT_EXPECT_EQ(test, cpufreq_cdev_set_state(C(ctx, 1)->cdev, 24), 0);
	KUNIT_EXPECT_EQ(test, qos_max(2), 384000);
	KUNIT_EXPECT_EQ(test, cpufreq_cdev_set_state(C(ctx, 1)->cdev, 25), -EINVAL);
	KUNIT_EXPECT_EQ(test, cpufreq_cdev_get_state(C(ctx, 1)->cdev, &state), 0);
	KUNIT_EXPECT_EQ(test, state, 24UL);
	KUNIT_EXPECT_EQ(test, cpufreq_cdev_set_state(C(ctx, 1)->cdev, 0), 0);
	KUNIT_EXPECT_EQ(test, qos_max(2), base2);
	KUNIT_EXPECT_EQ(test, cpufreq_cdev_set_state(C(ctx, 0)->cdev, 23), 0);
	device_release_driver(&f->consumer->dev);
	KUNIT_EXPECT_EQ(test, qos_max(0), base0);
	KUNIT_EXPECT_EQ(test, qos_max(2), base2);
}

static void consumer_before_cpufreq(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpufreq_cdev_ctx *ctx;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	settle(ctx);
	KUNIT_EXPECT_NULL(test, C(ctx, 0)->cdev);
	KUNIT_EXPECT_NULL(test, C(ctx, 1)->cdev);
	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	KUNIT_EXPECT_NOT_NULL(test, C(ctx, 0)->cdev);
	KUNIT_EXPECT_NOT_NULL(test, C(ctx, 1)->cdev);
}

/* Factory held the policy reference forever: driver unregister would block. */
static void cpufreq_driver_cycle(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpufreq_cdev_ctx *ctx;
	int i;

	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	settle(ctx);
	for (i = 0; i < 3; i++) {
		KUNIT_ASSERT_NOT_NULL(test, C(ctx, 0)->cdev);
		KUNIT_ASSERT_EQ(test, cpufreq_cdev_set_state(C(ctx, 0)->cdev, 7), 0);
		stop_cpufreq(f);
		KUNIT_EXPECT_NULL(test, C(ctx, 0)->cdev);
		KUNIT_EXPECT_NULL(test, C(ctx, 1)->cdev);
		KUNIT_EXPECT_NULL(test, C(ctx, 0)->policy);
		KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
		KUNIT_EXPECT_EQ(test, C(ctx, 0)->max_state, 23UL);
		KUNIT_EXPECT_EQ(test, C(ctx, 1)->max_state, 24UL);
	}
	KUNIT_EXPECT_NOT_NULL(test, C(ctx, 1)->cdev);
}

static void cpu_hotplug_keeps_cdev(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpufreq_cdev_ctx *ctx;
	struct thermal_cooling_device *before;

	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	settle(ctx);
	before = C(ctx, 1)->cdev;
	KUNIT_ASSERT_NOT_NULL(test, before);
	KUNIT_ASSERT_EQ(test, remove_cpu(3), 0);
	KUNIT_ASSERT_EQ(test, remove_cpu(2), 0);
	KUNIT_EXPECT_EQ(test, cpufreq_cdev_set_state(before, 10), 0);
	KUNIT_ASSERT_EQ(test, add_cpu(2), 0);
	KUNIT_ASSERT_EQ(test, add_cpu(3), 0);
	settle(ctx);
	KUNIT_EXPECT_PTR_EQ(test, C(ctx, 1)->cdev, before);
	KUNIT_EXPECT_EQ(test, qos_max(2), 2476800);
}

static void repeated_online_events(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpufreq_cdev_ctx *ctx;
	struct thermal_cooling_device *before;
	int i;

	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	settle(ctx);
	before = C(ctx, 0)->cdev;
	for (i = 0; i < 20; i++) {
		cpufreq_cdev_hp_online(0, &ctx->cpuhp_node);
		cpufreq_cdev_hp_online(2, &ctx->cpuhp_node);
	}
	settle(ctx);
	KUNIT_EXPECT_PTR_EQ(test, C(ctx, 0)->cdev, before);
}

static void consumer_rebind(struct kunit *test)
{
	struct fixture *f = test->priv;
	int i;

	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	for (i = 0; i < 3; i++) {
		struct cpufreq_cdev_ctx *ctx = ctx_of(f);

		KUNIT_ASSERT_NOT_NULL(test, ctx);
		settle(ctx);
		KUNIT_ASSERT_NOT_NULL(test, C(ctx, 0)->cdev);
		KUNIT_ASSERT_EQ(test, cpufreq_cdev_set_state(C(ctx, 0)->cdev, 3), 0);
		device_release_driver(&f->consumer->dev);
		KUNIT_ASSERT_EQ(test, device_attach(&f->consumer->dev), 1);
	}
}

static void unsorted_invalid_table(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct cpufreq_cdev_ctx *ctx;

	use_bad_table = true;
	KUNIT_ASSERT_EQ(test, start_cpufreq(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	ctx = ctx_of(f);
	KUNIT_ASSERT_NOT_NULL(test, ctx);
	settle(ctx);
	KUNIT_ASSERT_NOT_NULL(test, C(ctx, 0)->cdev);
	KUNIT_EXPECT_EQ(test, C(ctx, 0)->max_state, 2UL);
	KUNIT_EXPECT_EQ(test, C(ctx, 0)->freq_table[0], 2000000U);
	KUNIT_EXPECT_EQ(test, C(ctx, 0)->freq_table[1], 1000000U);
	KUNIT_EXPECT_EQ(test, C(ctx, 0)->freq_table[2], 300000U);
}

static void no_matching_cpu(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "nomatch"), 0);
	KUNIT_EXPECT_PTR_EQ(test, f->consumer->dev.driver, NULL);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(probe_saved_tables), KUNIT_CASE(limits_and_release),
	KUNIT_CASE(consumer_before_cpufreq), KUNIT_CASE(cpufreq_driver_cycle),
	KUNIT_CASE(cpu_hotplug_keeps_cdev), KUNIT_CASE(repeated_online_events),
	KUNIT_CASE(consumer_rebind), KUNIT_CASE(unsorted_invalid_table),
	KUNIT_CASE(no_matching_cpu), {}
};
static struct kunit_suite suite = {
	.name = "avr-cpufreq-cooling", .init = setup, .exit = teardown, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
