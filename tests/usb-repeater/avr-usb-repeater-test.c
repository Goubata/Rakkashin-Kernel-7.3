// SPDX-License-Identifier: GPL-2.0-only
/* Native registry/devres/device/module reference tests; no USB hardware. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/usb/repeater.h>
struct fixture { struct usb_repeater r;struct device_node *node,*disabled;struct platform_device *pdev;struct device *consumer; };
static int provider_probe(struct platform_device *pdev)
{
 struct fixture *f=platform_get_drvdata(pdev);f->r.dev=&pdev->dev;return usb_add_repeater_dev(&f->r);
}
static void provider_remove(struct platform_device *pdev)
{
 struct fixture *f=platform_get_drvdata(pdev);usb_remove_repeater_dev(&f->r);
}
static struct platform_driver provider_driver={.probe=provider_probe,.remove=provider_remove,.driver={.name="avr-repeater-provider"}};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
 if(!f)return -ENOMEM;test->priv=f;
 f->node=of_find_node_by_path("/avr-repeater-test");f->disabled=of_find_node_by_path("/avr-repeater-disabled");
 if(!f->node || !f->disabled){ret=-ENODEV;goto nodes;}
 ret=platform_driver_register(&provider_driver);if(ret)goto nodes;
 f->pdev=platform_device_alloc("avr-repeater-provider",PLATFORM_DEVID_AUTO);if(!f->pdev){ret=-ENOMEM;goto driver;}
 f->pdev->dev.of_node=of_node_get(f->node);platform_set_drvdata(f->pdev,f);
 ret=platform_device_add(f->pdev);if(ret){platform_device_put(f->pdev);goto driver;}
 if(!f->pdev->dev.driver){ret=-ENODEV;goto device;}
 f->consumer=root_device_register("avr-repeater-consumer");if(IS_ERR(f->consumer)){ret=PTR_ERR(f->consumer);goto device;}
 return 0;
device:platform_device_unregister(f->pdev);
driver:platform_driver_unregister(&provider_driver);
nodes:of_node_put(f->node);of_node_put(f->disabled);return ret;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;root_device_unregister(f->consumer);platform_device_unregister(f->pdev);platform_driver_unregister(&provider_driver);of_node_put(f->node);of_node_put(f->disabled);
}
static unsigned int refs(struct fixture *f){return kref_read(&f->pdev->dev.kobj.kref);}
static void managed_reference_returns_to_baseline(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int before=refs(f);int mods=module_refcount(THIS_MODULE);
 void *group=devres_open_group(f->consumer,NULL,GFP_KERNEL);struct usb_repeater *r;
 KUNIT_ASSERT_NOT_NULL(test,group);r=devm_usb_get_repeater_by_node(f->consumer,f->node);
 KUNIT_EXPECT_PTR_EQ(test,r,&f->r);KUNIT_EXPECT_EQ(test,refs(f),before+1);KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),mods+1);
 devres_release_group(f->consumer,group);KUNIT_EXPECT_EQ(test,refs(f),before);KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),mods);
}
static void repeated_managed_acquisitions_release_independently(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int before=refs(f);int mods=module_refcount(THIS_MODULE),i;
 void *group=devres_open_group(f->consumer,NULL,GFP_KERNEL);KUNIT_ASSERT_NOT_NULL(test,group);
 for(i=0;i<32;i++)KUNIT_EXPECT_PTR_EQ(test,devm_usb_get_repeater_by_node(f->consumer,f->node),&f->r);
 KUNIT_EXPECT_EQ(test,refs(f),before+32);devres_release_group(f->consumer,group);KUNIT_EXPECT_EQ(test,refs(f),before);KUNIT_EXPECT_EQ(test,module_refcount(THIS_MODULE),mods);
}
static void invalid_and_disabled_nodes_do_not_acquire(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int before=refs(f);
 KUNIT_EXPECT_EQ(test,PTR_ERR(devm_usb_get_repeater_by_node(f->consumer,NULL)),(long)-EINVAL);
 KUNIT_EXPECT_EQ(test,PTR_ERR(devm_usb_get_repeater_by_node(f->consumer,f->disabled)),(long)-ENODEV);
 KUNIT_EXPECT_EQ(test,refs(f),before);
}
static void duplicates_and_repeated_remove_are_bounded(struct kunit *test)
{
 struct fixture *f=test->priv;struct usb_repeater other={.dev=&f->pdev->dev};
 KUNIT_EXPECT_EQ(test,usb_add_repeater_dev(&f->r),-EEXIST);KUNIT_EXPECT_EQ(test,usb_add_repeater_dev(&other),-EEXIST);
 usb_remove_repeater_dev(&f->r);usb_remove_repeater_dev(&f->r);
 KUNIT_EXPECT_EQ(test,PTR_ERR(usb_get_repeater_by_node(f->node)),(long)-EPROBE_DEFER);
 KUNIT_EXPECT_EQ(test,usb_add_repeater_dev(&f->r),0);
 KUNIT_EXPECT_PTR_EQ(test,usb_get_repeater_by_node(f->node),&f->r);
}
static void missing_phandle_optional_and_required(struct kunit *test)
{
 struct fixture *f=test->priv;
 f->consumer->of_node=of_node_get(f->node);
 KUNIT_EXPECT_EQ(test,PTR_ERR(devm_usb_get_repeater_by_phandle(f->consumer,"absent",0)),(long)-ENODEV);
 KUNIT_EXPECT_PTR_EQ(test,devm_usb_get_optional_repeater_by_phandle(f->consumer,"absent",0),NULL);
 of_node_put(f->consumer->of_node);f->consumer->of_node=NULL;
}
static struct kunit_case cases[]={KUNIT_CASE(managed_reference_returns_to_baseline),KUNIT_CASE(repeated_managed_acquisitions_release_independently),KUNIT_CASE(invalid_and_disabled_nodes_do_not_acquire),KUNIT_CASE(duplicates_and_repeated_remove_are_bounded),KUNIT_CASE(missing_phandle_optional_and_required),{}};
static struct kunit_suite suite={.name="avr-usb-repeater",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
