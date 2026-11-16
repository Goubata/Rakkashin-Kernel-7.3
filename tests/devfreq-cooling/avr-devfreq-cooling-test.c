// SPDX-License-Identifier: GPL-2.0-only
/* Full driver on native OF/platform/devres/device-link/OPP/PM-QoS/thermal paths.
 * The private "GPU" supplier only publishes the saved AVR 21-level OPP table. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#undef pr_fmt
#undef module_driver
#define module_driver(driver, register, unregister)
#include "../../ports/qti-devfreq-cdev/qti_devfreq_cdev.c"

/* Saved running.dtb qcom,gpu-pwrlevels-0 (user undervolt/OC table), Hz. */
static const unsigned long saved_gpu_hz[] = {
	1238000000, 1225000000, 1150000000, 1075000000, 1025000000, 975000000,
	891000000, 795000000, 732000000, 714000000, 660000000, 589000000,
	496000000, 456000000, 403000000, 301000000, 250000000, 195000000,
	150000000, 100000000, 72000000,
};

static int gpu_probe(struct platform_device *pdev)
{
	u32 count = ARRAY_SIZE(saved_gpu_hz);
	int i, ret;

	of_property_read_u32(pdev->dev.of_node, "avr,opp-count", &count);
	for (i = 0; i < count && i < ARRAY_SIZE(saved_gpu_hz); i++) {
		ret = dev_pm_opp_add(&pdev->dev, saved_gpu_hz[i], 0);
		if (ret) {
			dev_pm_opp_remove_all_dynamic(&pdev->dev);
			return ret;
		}
	}
	return 0;
}

static void gpu_remove(struct platform_device *pdev)
{
	dev_pm_opp_remove_all_dynamic(&pdev->dev);
}

static const struct of_device_id gpu_match[] = { { .compatible = "avr,gpu-private" }, {} };
static struct platform_driver gpu_driver = {
	.probe = gpu_probe, .remove = gpu_remove,
	.driver = { .name = "avr-gpu-private", .of_match_table = gpu_match },
};
static const struct of_device_id consumer_match[] = { { .compatible = "avr,gpu-cdev-private" }, {} };

struct fixture {
	struct platform_device *gpu[2], *consumer;
	bool gpu_driver, consumer_driver;
};

static int add_device(const char *name, const char *path, struct platform_device **result)
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
	*result = pdev;
	wait_for_device_probe();
	return 0;
}

static int add_consumer(struct fixture *f, const char *node)
{
	char path[96];

	snprintf(path, sizeof(path), "/avr-gpu-fixture/%s", node);
	return add_device("avr-gpu-cdev-private", path, &f->consumer);
}

static int start_gpu_driver(struct fixture *f)
{
	int ret = platform_driver_register(&gpu_driver);

	if (!ret)
		f->gpu_driver = true;
	wait_for_device_probe();
	return ret;
}

static s32 qos_max(struct fixture *f)
{
	return dev_pm_qos_read_value(&f->gpu[0]->dev, DEV_PM_QOS_MAX_FREQUENCY);
}

static struct devfreq_cdev_device *bound(struct fixture *f)
{
	return f->consumer ? platform_get_drvdata(f->consumer) : NULL;
}

static void release(struct fixture *f)
{
	int i;

	if (f->consumer) {
		platform_device_unregister(f->consumer);
		f->consumer = NULL;
	}
	if (f->consumer_driver)
		platform_driver_unregister(&devfreq_cdev_driver);
	f->consumer_driver = false;
	for (i = 0; i < 2; i++) {
		if (f->gpu[i])
			platform_device_unregister(f->gpu[i]);
		f->gpu[i] = NULL;
	}
	if (f->gpu_driver)
		platform_driver_unregister(&gpu_driver);
	f->gpu_driver = false;
}

static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	int ret;

	if (!f)
		return -ENOMEM;
	test->priv = f;
	ret = add_device("avr-gpu-private", "/avr-gpu-fixture/gpu", &f->gpu[0]);
	if (!ret)
		ret = add_device("avr-gpu-private", "/avr-gpu-fixture/gpu-empty", &f->gpu[1]);
	if (ret)
		goto fail;
	devfreq_cdev_driver.driver.name = "avr-gpu-cdev-private";
	devfreq_cdev_driver.driver.of_match_table = consumer_match;
	ret = platform_driver_register(&devfreq_cdev_driver);
	if (ret)
		goto fail;
	f->consumer_driver = true;
	return 0;
fail:
	release(f);
	return ret;
}

static void teardown(struct kunit *test)
{
	struct fixture *f = test->priv;

	if (f->consumer) {
		platform_device_unregister(f->consumer);
		f->consumer = NULL;
	}
	if (f->gpu_driver)
		KUNIT_EXPECT_EQ(test, qos_max(f), PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	release(f);
}

static void native_probe_saved_table(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct devfreq_cdev_device *d;
	unsigned long state = 99;
	int i;

	KUNIT_ASSERT_EQ(test, start_gpu_driver(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	d = bound(f);
	KUNIT_ASSERT_NOT_NULL(test, d);
	KUNIT_EXPECT_STREQ(test, d->cdev->type, "gpu");
	KUNIT_EXPECT_EQ(test, d->max_state, 20U);
	for (i = 0; i < ARRAY_SIZE(saved_gpu_hz); i++)
		KUNIT_EXPECT_EQ(test, d->freq_table[i], saved_gpu_hz[i] / 1000);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_get_max_state(d->cdev, &state), 0);
	KUNIT_EXPECT_EQ(test, state, 20UL);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_get_state(d->cdev, &state), 0);
	KUNIT_EXPECT_EQ(test, state, 0UL);
	KUNIT_EXPECT_EQ(test, qos_max(f), PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
}

static void limit_and_release(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct devfreq_cdev_device *d;
	unsigned long state;

	KUNIT_ASSERT_EQ(test, start_gpu_driver(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	d = bound(f);
	KUNIT_ASSERT_NOT_NULL(test, d);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 6), 0);
	KUNIT_EXPECT_EQ(test, qos_max(f), 891000);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 20), 0);
	KUNIT_EXPECT_EQ(test, qos_max(f), 72000);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 20), 0);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 21), -EINVAL);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_get_state(d->cdev, &state), 0);
	KUNIT_EXPECT_EQ(test, state, 20UL);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 0), 0);
	KUNIT_EXPECT_EQ(test, qos_max(f), 1238000);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 14), 0);
	device_release_driver(&f->consumer->dev);
	KUNIT_EXPECT_EQ(test, qos_max(f), PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
}

/* KGSL keeps its own max-frequency request; aggregation is native min(). */
static void coexists_with_kgsl_request(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct dev_pm_qos_request kgsl = {};
	struct devfreq_cdev_device *d;
	unsigned long state;

	KUNIT_ASSERT_EQ(test, start_gpu_driver(f), 0);
	KUNIT_ASSERT_GE(test, dev_pm_qos_add_request(&f->gpu[0]->dev, &kgsl,
			DEV_PM_QOS_MAX_FREQUENCY, 589000), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	d = bound(f);
	KUNIT_ASSERT_NOT_NULL(test, d);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 2), 0);
	KUNIT_EXPECT_EQ(test, qos_max(f), 589000);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_get_state(d->cdev, &state), 0);
	KUNIT_EXPECT_EQ(test, state, 2UL);
	KUNIT_EXPECT_EQ(test, devfreq_cdev_set_state(d->cdev, 15), 0);
	KUNIT_EXPECT_EQ(test, qos_max(f), 301000);
	device_release_driver(&f->consumer->dev);
	KUNIT_EXPECT_EQ(test, qos_max(f), 589000);
	dev_pm_qos_remove_request(&kgsl);
}

static void defers_until_gpu_bound(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	KUNIT_EXPECT_NULL(test, bound(f));
	KUNIT_ASSERT_EQ(test, start_gpu_driver(f), 0);
	KUNIT_EXPECT_NOT_NULL(test, bound(f));
	KUNIT_EXPECT_EQ(test, bound(f) ? bound(f)->max_state : 0, 20U);
}

static void gpu_unbind_unbinds_consumer(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct devfreq_cdev_device *d;
	int i;

	KUNIT_ASSERT_EQ(test, start_gpu_driver(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	for (i = 0; i < 3; i++) {
		d = bound(f);
		KUNIT_ASSERT_NOT_NULL(test, d);
		KUNIT_ASSERT_EQ(test, devfreq_cdev_set_state(d->cdev, 10), 0);
		KUNIT_EXPECT_EQ(test, qos_max(f), 660000);
		device_release_driver(&f->gpu[0]->dev);
		KUNIT_EXPECT_NULL(test, bound(f));
		KUNIT_EXPECT_EQ(test, qos_max(f), PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		KUNIT_ASSERT_EQ(test, device_attach(&f->gpu[0]->dev), 1);
		wait_for_device_probe();
	}
	KUNIT_EXPECT_NOT_NULL(test, bound(f));
}

static void consumer_rebind(struct kunit *test)
{
	struct fixture *f = test->priv;
	int i;

	KUNIT_ASSERT_EQ(test, start_gpu_driver(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	for (i = 0; i < 3; i++) {
		struct devfreq_cdev_device *d = bound(f);

		KUNIT_ASSERT_NOT_NULL(test, d);
		KUNIT_ASSERT_EQ(test, devfreq_cdev_set_state(d->cdev, 3), 0);
		device_release_driver(&f->consumer->dev);
		KUNIT_EXPECT_EQ(test, qos_max(f), PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
		KUNIT_ASSERT_EQ(test, device_attach(&f->consumer->dev), 1);
	}
}

static void reject(struct kunit *test, const char *name)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, start_gpu_driver(f), 0);
	KUNIT_ASSERT_EQ(test, add_consumer(f, name), 0);
	KUNIT_EXPECT_PTR_EQ(test, f->consumer->dev.driver, NULL);
	KUNIT_EXPECT_NULL(test, bound(f));
}

static void missing_phandle(struct kunit *test) { reject(test, "no-phandle"); }
static void empty_opp_table(struct kunit *test) { reject(test, "empty"); }
static void phandle_without_device(struct kunit *test) { reject(test, "no-device"); }

static struct kunit_case cases[] = {
	KUNIT_CASE(native_probe_saved_table), KUNIT_CASE(limit_and_release),
	KUNIT_CASE(coexists_with_kgsl_request), KUNIT_CASE(defers_until_gpu_bound),
	KUNIT_CASE(gpu_unbind_unbinds_consumer), KUNIT_CASE(consumer_rebind),
	KUNIT_CASE(missing_phandle), KUNIT_CASE(empty_opp_table),
	KUNIT_CASE(phandle_without_device), {}
};
static struct kunit_suite suite = {
	.name = "avr-devfreq-cooling", .init = setup, .exit = teardown, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
