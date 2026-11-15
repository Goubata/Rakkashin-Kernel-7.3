// SPDX-License-Identifier: GPL-2.0-only
/* Real OF platform population/probe and relay API. No synthetic platform device
 * registration in this test and no physical AVR device/firmware access. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/of_platform.h>
#include <linux/mmi_relay.h>
struct observation {struct notifier_block nb;unsigned long value;void *payload;unsigned int calls;};
static int callback(struct notifier_block *nb,unsigned long value,void *data)
{
	struct observation *o=container_of(nb,struct observation,nb);o->value=value;o->payload=data;o->calls++;return NOTIFY_OK;
}
static void firmware_node_populates_and_binds_real_provider(struct kunit *test)
{
	struct device_node *node=of_find_node_by_path("/avr-test-bus/mmi-relay");
	struct platform_device *pdev;struct observation o={.nb.notifier_call=callback};int ret;
	KUNIT_ASSERT_NOT_NULL(test,node);
	KUNIT_EXPECT_TRUE(test,of_device_is_compatible(node,"mmi,sys-relay"));
	pdev=of_find_device_by_node(node);of_node_put(node);
	KUNIT_ASSERT_NOT_NULL(test,pdev);
	if(!pdev->dev.driver){KUNIT_FAIL(test,"OF-created relay device did not bind");goto out;}
	KUNIT_EXPECT_STREQ(test,pdev->dev.driver->name,"mmi_relay");
	KUNIT_EXPECT_NOT_NULL(test,platform_get_drvdata(pdev));
	ret=relay_register_action(BLOCKING,FPS,&o.nb);KUNIT_EXPECT_EQ(test,ret,0);if(ret)goto out;
	KUNIT_EXPECT_EQ(test,relay_notifier_fire(BLOCKING,FPS,144,&o),NOTIFY_OK);
	KUNIT_EXPECT_EQ(test,o.calls,1U);KUNIT_EXPECT_EQ(test,o.value,144UL);KUNIT_EXPECT_PTR_EQ(test,o.payload,&o);
	KUNIT_EXPECT_EQ(test,relay_unregister_action(BLOCKING,FPS,&o.nb),0);
out:
	put_device(&pdev->dev);
}
static struct kunit_case cases[]={KUNIT_CASE(firmware_node_populates_and_binds_real_provider),{}};
static struct kunit_suite suite={.name="avr-moto-touch-of",.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("Real OF relay broker population and API test");
