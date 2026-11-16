// SPDX-License-Identifier: GPL-2.0-only
/* Full driver on native OF/platform/devres/thermal_of with a private sensor/zone. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#undef pr_fmt
#undef module_driver
#define module_driver(driver, register, unregister)
#include "../../ports/qti-userspace-cdev/qti_userspace_cdev.c"

static const struct of_device_id consumer_match[] = { { .compatible = "avr,userspace-cdev-private" }, {} };

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

static int add_consumer(struct fixture *f, const char *node)
{
	char path[64];

	snprintf(path, sizeof(path), "/avr-us-fixture/%s", node);
	return add_pdev("avr-userspace-cdev-private", path, &f->consumer);
}

static struct userspace_cdev *inst(struct fixture *f, const char *name)
{
	struct userspace_cdev *u = f->consumer ? platform_get_drvdata(f->consumer) : NULL;
	int i;

	for (i = 0; u && i < 2; i++)
		if (!strcmp(u[i].cdev_name, name))
			return &u[i];
	return NULL;
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
	int ret;

	if (!f)
		return -ENOMEM;
	test->priv = f;
	userspace_cdev_driver.driver.name = "avr-userspace-cdev-private";
	userspace_cdev_driver.driver.of_match_table = consumer_match;
	ret = platform_driver_register(&userspace_cdev_driver);
	if (!ret)
		f->driver = true;
	return ret;
}

static void teardown(struct kunit *test)
{
	struct fixture *f = test->priv;

	if (f->sensor)
		platform_device_unregister(f->sensor);
	if (f->consumer)
		platform_device_unregister(f->consumer);
	if (f->driver)
		platform_driver_unregister(&userspace_cdev_driver);
}

static void probe_children(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct userspace_cdev *fps, *other;
	unsigned long v;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	fps = inst(f, "display-fps");
	other = inst(f, "other-cdev");
	KUNIT_ASSERT_NOT_NULL(test, fps);
	KUNIT_ASSERT_NOT_NULL(test, other);
	KUNIT_ASSERT_NOT_NULL(test, fps->cdev);
	KUNIT_EXPECT_STREQ(test, fps->cdev->type, "display-fps");
	KUNIT_EXPECT_PTR_EQ(test, fps->cdev->np, fps->np);
	KUNIT_EXPECT_EQ(test, userspace_get_max_state(fps->cdev, &v), 0);
	KUNIT_EXPECT_EQ(test, v, 16UL);
	KUNIT_EXPECT_EQ(test, userspace_get_max_state(other->cdev, &v), 0);
	KUNIT_EXPECT_EQ(test, v, 3UL);
}

static void level_store(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct userspace_cdev *fps;
	unsigned long v;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	fps = inst(f, "display-fps");
	KUNIT_ASSERT_NOT_NULL(test, fps);
	KUNIT_EXPECT_EQ(test, userspace_set_cur_state(fps->cdev, 16), 0);
	KUNIT_EXPECT_EQ(test, userspace_get_cur_state(fps->cdev, &v), 0);
	KUNIT_EXPECT_EQ(test, v, 16UL);
	KUNIT_EXPECT_EQ(test, userspace_set_cur_state(fps->cdev, 17), -EINVAL);
	KUNIT_EXPECT_EQ(test, userspace_get_cur_state(fps->cdev, &v), 0);
	KUNIT_EXPECT_EQ(test, v, 16UL);
}

/* Saved maps select display-fps 1/2/3 per trip; step_wise must reach each level. */
static void dt_maps_select_levels(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct userspace_cdev *fps;
	static const int temps[] = { 42000, 47000, 52000, 30000 };
	static const unsigned long want[] = { 1, 2, 3, 0 };
	unsigned long v;
	int i, n;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	fps = inst(f, "display-fps");
	KUNIT_ASSERT_NOT_NULL(test, fps);
	f->temp = 25000;
	KUNIT_ASSERT_EQ(test, add_pdev("avr-us-temp", "/avr-us-sensor", &f->sensor), 0);
	f->zone = devm_thermal_of_zone_register(&f->sensor->dev, 0, &f->temp, &recorded_ops);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->zone));
	for (i = 0; i < ARRAY_SIZE(temps); i++) {
		WRITE_ONCE(f->temp, temps[i]);
		/* step_wise moves one step per evaluation; settle with a few passes. */
		for (n = 0; n < 4; n++)
			thermal_zone_device_update(f->zone, THERMAL_EVENT_UNSPECIFIED);
		KUNIT_EXPECT_EQ(test, userspace_get_cur_state(fps->cdev, &v), 0);
		KUNIT_EXPECT_EQ(test, v, want[i]);
	}
}

static void rebind_releases_nodes(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct device_node *np = of_find_node_by_path("/avr-us-fixture/good/display-fps");
	int i;

	KUNIT_ASSERT_NOT_NULL(test, np);
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	for (i = 0; i < 3; i++) {
		KUNIT_ASSERT_NOT_NULL(test, inst(f, "display-fps"));
		device_release_driver(&f->consumer->dev);
		KUNIT_EXPECT_NULL(test, platform_get_drvdata(f->consumer));
		KUNIT_ASSERT_EQ(test, device_attach(&f->consumer->dev), 1);
	}
	of_node_put(np);
}

static void missing_level_unwinds(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "missing-level"), 0);
	KUNIT_EXPECT_PTR_EQ(test, f->consumer->dev.driver, NULL);
	/* The first child's cdev was unregistered by devres: rebinding the good set works. */
	platform_device_unregister(f->consumer);
	f->consumer = NULL;
	KUNIT_ASSERT_EQ(test, add_consumer(f, "good"), 0);
	KUNIT_EXPECT_NOT_NULL(test, inst(f, "display-fps"));
}

static void no_children_rejected(struct kunit *test)
{
	struct fixture *f = test->priv;

	KUNIT_ASSERT_EQ(test, add_consumer(f, "empty"), 0);
	KUNIT_EXPECT_PTR_EQ(test, f->consumer->dev.driver, NULL);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(probe_children), KUNIT_CASE(level_store), KUNIT_CASE(dt_maps_select_levels),
	KUNIT_CASE(rebind_releases_nodes), KUNIT_CASE(missing_level_unwinds),
	KUNIT_CASE(no_children_rejected), {}
};
static struct kunit_suite suite = {
	.name = "avr-userspace-cdev", .init = setup, .exit = teardown, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
