// SPDX-License-Identifier: GPL-2.0-only
/* Production modules only. Guest has no SMEM hardware/firmware backend. */
#include <kunit/test.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smem.h>
#include <linux/mmi_device.h>
#include <soc/qcom/mmi_boot_info.h>

static int match_component(struct device *dev, const void *name)
{
	return !strcmp(to_platform_device(dev)->name, name);
}

static void independent_native_deferred_components(struct kunit *test)
{
	const char *names[] = { "mmi_info_storage", "mmi_info_ram",
		"mmi_info_unit", "mmi_info_boot" };
	struct device *dev;
	size_t size = 0;
	void *item = qcom_smem_get(QCOM_SMEM_HOST_ANY, 135, &size);
	int i;

	KUNIT_ASSERT_TRUE(test, IS_ERR(item));
	KUNIT_EXPECT_EQ(test, PTR_ERR(item), (long)-EPROBE_DEFER);
	for (i = 0; i < ARRAY_SIZE(names); i++) {
		dev = bus_find_device(&platform_bus_type, NULL, names[i], match_component);
		KUNIT_ASSERT_NOT_NULL(test, dev);
		device_lock(dev);
		if (i == 0 || i == 3) {
			KUNIT_EXPECT_NOT_NULL(test, dev->driver);
			KUNIT_EXPECT_NOT_NULL(test, dev_get_drvdata(dev));
		} else {
			KUNIT_EXPECT_PTR_EQ(test, dev->driver, NULL);
			KUNIT_EXPECT_PTR_EQ(test, dev_get_drvdata(dev), NULL);
		}
		device_unlock(dev);
		put_device(dev);
	}
}

static void public_metadata_without_ready_smem(struct kunit *test)
{
	struct device_node *node;
	struct file *file;
	char buf[32] = {};
	loff_t pos = 0;
	ssize_t count;

	KUNIT_EXPECT_STREQ(test, bi_bootmode(), "normal");
	KUNIT_EXPECT_STREQ(test, bi_bootreason(), "test-boot");
	node = of_find_node_by_path("/avr-status-exact");
	KUNIT_ASSERT_NOT_NULL(test, node);
	KUNIT_EXPECT_TRUE(test, mmi_device_is_available(node));
	of_node_put(node);
	file = filp_open("/sys/storage/type", O_RDONLY, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR(file));
	count = kernel_read(file, buf, sizeof(buf) - 1, &pos);
	filp_close(file, NULL);
	KUNIT_EXPECT_GT(test, count, (ssize_t)0);
	KUNIT_EXPECT_STREQ(test, buf, "UFS\n");
}

static struct kunit_case cases[] = {
	KUNIT_CASE(independent_native_deferred_components),
	KUNIT_CASE(public_metadata_without_ready_smem), {}
};
static struct kunit_suite suite = { .name = "avr-mmi-info-native", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Production info module with real unready SMEM and native lifecycle");
