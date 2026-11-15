// SPDX-License-Identifier: GPL-2.0-only
/* Unmodified production modules, real OF probe and real minidump pending APIs.
 * Two loads test registration before and removal after production driver unload. */
#include <kunit/test.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <soc/qcom/minidump.h>
static bool expect_present=true;
module_param(expect_present,bool,0400);
static void actual_pending_registration_follows_provider_lifetime(struct kunit *test)
{
	struct md_region entry={.phys_addr=0x60000000,.size=4096};
	struct device_node *node;struct platform_device *pdev;void *mapping;int ret;
	KUNIT_ASSERT_FALSE(test,msm_minidump_enabled()); /* Only the pending path, no firmware backend. */
	node=of_find_node_by_path("/avr-native-annotate");KUNIT_ASSERT_NOT_NULL(test,node);
	pdev=of_find_device_by_node(node);of_node_put(node);KUNIT_ASSERT_NOT_NULL(test,pdev);
	if(expect_present) {
		KUNIT_EXPECT_NOT_NULL(test,pdev->dev.driver);
		if(pdev->dev.driver)KUNIT_EXPECT_STREQ(test,pdev->dev.driver->name,"mmi_annotate");
		KUNIT_EXPECT_NOT_NULL(test,platform_get_drvdata(pdev));
	} else {
		KUNIT_EXPECT_PTR_EQ(test,pdev->dev.driver,NULL);
		KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(pdev),NULL);
	}
	put_device(&pdev->dev);
	mapping=memremap(0x60000000,4096,MEMREMAP_WB);KUNIT_ASSERT_NOT_NULL(test,mapping);
	entry.virt_addr=(uintptr_t)mapping;strscpy(entry.name,"ANNOTATE",sizeof(entry.name));
	ret=msm_minidump_add_region(&entry);
	if(expect_present)KUNIT_EXPECT_EQ(test,ret,-EEXIST);else KUNIT_EXPECT_GE(test,ret,0);
	if(ret>=0)KUNIT_EXPECT_EQ(test,msm_minidump_remove_region(&entry),0);
	memunmap(mapping);
}
static struct kunit_case cases[]={KUNIT_CASE(actual_pending_registration_follows_provider_lifetime),{}};
static struct kunit_suite suite={.name="avr-mmi-annotate-native",.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("Production annotate and real minidump pending lifecycle test");
