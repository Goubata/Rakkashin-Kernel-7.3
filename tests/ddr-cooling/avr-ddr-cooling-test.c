// SPDX-License-Identifier: GPL-2.0-only
/* Full driver, native OF/platform/devres/ICC paths and thermal cdev registry.
 * Only the provider's hardware set operation records requests/faults in RAM. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/interconnect-provider.h>
#undef module_driver
#define module_driver(driver, register, unregister)
#include "../../ports/qcom-ddr-cooling/ddr_cdev.c"
struct fixture {
 struct platform_device *supplier,*consumer;
 struct icc_provider provider;
 struct icc_node *nodes[2];
 bool supplier_driver,consumer_driver,registered,initialized;
 int fail,calls;
 u32 peak;
};
static int recorded_set(struct icc_node *src,struct icc_node *dst)
{
 struct fixture *f=src->provider->data;
 f->calls++;
 if(READ_ONCE(f->fail))return -EIO;
 f->peak=dst->peak_bw;return 0;
}
/* The private provider starts with a recorded zero hardware vote. Omitting
 * get_bw makes native ICC conservatively retain an INT_MAX boot request. */
static int recorded_get_bw(struct icc_node *node,u32 *avg,u32 *peak)
{*avg=0;*peak=0;return 0;}
static struct icc_node *translate(const struct of_phandle_args *spec,void *data)
{
 struct fixture *f=data;
 if(spec->args_count!=1)return ERR_PTR(-EINVAL);
 if(spec->args[0]==3)return f->nodes[0];
 if(spec->args[0]==512)return f->nodes[1];
 return ERR_PTR(-EINVAL);
}
static int supplier_probe(struct platform_device *pdev){return 0;}
static const struct of_device_id supplier_match[]={{.compatible="avr,ddr-icc"},{}};
static struct platform_driver supplier_driver={.probe=supplier_probe,.driver={.name="avr-ddr-icc",.of_match_table=supplier_match}};
static const struct of_device_id consumer_match[]={{.compatible="avr,ddr-private"},{}};
static int add_device(const char *name,const char *path,struct platform_device **result)
{
 struct device_node *np=of_find_node_by_path(path);struct platform_device *pdev;int ret;
 if(!np)return -ENOENT;
 pdev=platform_device_alloc(name,PLATFORM_DEVID_AUTO);if(!pdev){of_node_put(np);return -ENOMEM;}
 device_set_node(&pdev->dev,of_fwnode_handle(np));ret=platform_device_add(pdev);
 if(ret){platform_device_put(pdev);return ret;}*result=pdev;wait_for_device_probe();return 0;
}
static int add_consumer(struct fixture *f,const char *node)
{
 char path[96];snprintf(path,sizeof(path),"/avr-ddr-fixture/%s",node);
 return add_device("avr-ddr-private",path,&f->consumer);
}
static void stop_consumer(struct fixture *f)
{if(f->consumer){platform_device_unregister(f->consumer);f->consumer=NULL;}}
static void release(struct fixture *f)
{
 WRITE_ONCE(f->fail,0);stop_consumer(f);
 if(f->consumer_driver)platform_driver_unregister(&ddr_cdev_driver);
 if(f->registered)icc_provider_deregister(&f->provider);
 if(f->initialized)icc_nodes_remove(&f->provider);
 if(f->supplier)platform_device_unregister(f->supplier);
 if(f->supplier_driver)platform_driver_unregister(&supplier_driver);
}
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret,i;
 if(!f)return -ENOMEM;test->priv=f;
 ret=platform_driver_register(&supplier_driver);if(ret)return ret;f->supplier_driver=true;
 ret=add_device("avr-ddr-icc","/avr-ddr-fixture/icc",&f->supplier);if(ret)goto fail;
 f->provider.dev=&f->supplier->dev;f->provider.data=f;f->provider.set=recorded_set;
 f->provider.get_bw=recorded_get_bw;f->provider.aggregate=icc_std_aggregate;f->provider.xlate=translate;
 icc_provider_init(&f->provider);f->initialized=true;
 for(i=0;i<2;i++){
  struct icc_node *n=icc_node_create_dyn();if(IS_ERR(n)){ret=PTR_ERR(n);goto fail;}
  f->nodes[i]=n;icc_node_add(n,&f->provider);
 }
 ret=icc_link_create(f->nodes[0],f->nodes[1]->id);if(ret)goto fail;
 ret=icc_provider_register(&f->provider);if(ret)goto fail;f->registered=true;icc_sync_state(&f->supplier->dev);
 ddr_cdev_driver.driver.name="avr-ddr-private";ddr_cdev_driver.driver.of_match_table=consumer_match;
 ret=platform_driver_register(&ddr_cdev_driver);if(ret)goto fail;f->consumer_driver=true;
 return 0;
fail:
 release(f);return ret;
}
static void teardown(struct kunit *test)
{
 struct fixture *f=test->priv;
 WRITE_ONCE(f->fail,0);stop_consumer(f);
 KUNIT_EXPECT_EQ(test,f->provider.users,0);KUNIT_EXPECT_EQ(test,f->peak,0U);
 release(f);
}
static void native_probe(struct kunit *test)
{
 struct fixture *f=test->priv;struct ddr_cdev *d;unsigned long state;
 KUNIT_ASSERT_EQ(test,add_consumer(f,"good"),0);d=platform_get_drvdata(f->consumer);KUNIT_ASSERT_NOT_NULL(test,d);
 KUNIT_EXPECT_STREQ(test,d->cdev->type,"ddr-cdev");KUNIT_EXPECT_EQ(test,d->freq_table[0],0U);KUNIT_EXPECT_EQ(test,d->freq_table[1],3552U);
 KUNIT_EXPECT_EQ(test,ddr_get_max_state(d->cdev,&state),0);KUNIT_EXPECT_EQ(test,state,1UL);
 KUNIT_EXPECT_EQ(test,ddr_get_cur_state(d->cdev,&state),0);KUNIT_EXPECT_EQ(test,state,0UL);KUNIT_EXPECT_EQ(test,f->provider.users,2);
}
static void actual_vote_and_release(struct kunit *test)
{
 struct fixture *f=test->priv;struct ddr_cdev *d;int calls;
 KUNIT_ASSERT_EQ(test,add_consumer(f,"good"),0);d=platform_get_drvdata(f->consumer);KUNIT_ASSERT_NOT_NULL(test,d);
 KUNIT_ASSERT_EQ(test,ddr_set_cur_state(d->cdev,1),0);KUNIT_EXPECT_EQ(test,f->peak,3552U);
 calls=f->calls;KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,1),0);KUNIT_EXPECT_EQ(test,f->calls,calls);
 KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,~0UL),-EINVAL);KUNIT_EXPECT_EQ(test,f->calls,calls);
 stop_consumer(f);KUNIT_EXPECT_EQ(test,f->peak,0U);KUNIT_EXPECT_EQ(test,f->provider.users,0);
}
static void failed_vote_retries(struct kunit *test)
{
 struct fixture *f=test->priv;struct ddr_cdev *d;unsigned long state=77;int calls;
 KUNIT_ASSERT_EQ(test,add_consumer(f,"good"),0);d=platform_get_drvdata(f->consumer);KUNIT_ASSERT_NOT_NULL(test,d);
 WRITE_ONCE(f->fail,1);KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,1),-EIO);
 KUNIT_EXPECT_EQ(test,ddr_get_cur_state(d->cdev,&state),-EAGAIN);KUNIT_EXPECT_EQ(test,state,77UL);calls=f->calls;
 KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,1),-EIO);KUNIT_EXPECT_GT(test,f->calls,calls);
 cancel_delayed_work_sync(&d->retry);WRITE_ONCE(f->fail,0);
 mod_delayed_work(system_dfl_wq,&d->retry,0);flush_delayed_work(&d->retry);
 KUNIT_EXPECT_EQ(test,ddr_get_cur_state(d->cdev,&state),0);KUNIT_EXPECT_EQ(test,state,1UL);KUNIT_EXPECT_EQ(test,f->peak,3552U);
}
static void cancel_pending_with_new_target(struct kunit *test)
{
 struct fixture *f=test->priv;struct ddr_cdev *d;
 KUNIT_ASSERT_EQ(test,add_consumer(f,"good"),0);d=platform_get_drvdata(f->consumer);KUNIT_ASSERT_NOT_NULL(test,d);
 WRITE_ONCE(f->fail,1);KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,1),-EIO);WRITE_ONCE(f->fail,0);
 KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,0),0);KUNIT_EXPECT_FALSE(test,delayed_work_pending(&d->retry));KUNIT_EXPECT_EQ(test,f->peak,0U);
}
static void pending_remove(struct kunit *test)
{
 struct fixture *f=test->priv;struct ddr_cdev *d;
 KUNIT_ASSERT_EQ(test,add_consumer(f,"good"),0);d=platform_get_drvdata(f->consumer);KUNIT_ASSERT_NOT_NULL(test,d);
 WRITE_ONCE(f->fail,1);KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,1),-EIO);WRITE_ONCE(f->fail,0);
 stop_consumer(f);KUNIT_EXPECT_EQ(test,f->provider.users,0);KUNIT_EXPECT_EQ(test,f->peak,0U);
}
static void native_rebind(struct kunit *test)
{
 struct fixture *f=test->priv;int i;
 KUNIT_ASSERT_EQ(test,add_consumer(f,"good"),0);
 for(i=0;i<3;i++){
  struct ddr_cdev *d=platform_get_drvdata(f->consumer);KUNIT_ASSERT_NOT_NULL(test,d);KUNIT_ASSERT_EQ(test,ddr_set_cur_state(d->cdev,1),0);
  device_release_driver(&f->consumer->dev);KUNIT_EXPECT_EQ(test,f->provider.users,0);KUNIT_EXPECT_EQ(test,f->peak,0U);
  KUNIT_ASSERT_EQ(test,device_attach(&f->consumer->dev),1);
 }
}
static void multilevel_table(struct kunit *test)
{
 struct fixture *f=test->priv;struct ddr_cdev *d;
 KUNIT_ASSERT_EQ(test,add_consumer(f,"multi"),0);d=platform_get_drvdata(f->consumer);KUNIT_ASSERT_NOT_NULL(test,d);
 KUNIT_EXPECT_EQ(test,d->max_state,2U);KUNIT_EXPECT_EQ(test,ddr_set_cur_state(d->cdev,2),0);KUNIT_EXPECT_EQ(test,f->peak,1776U);
}
static void reject(struct kunit *test,const char *name)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,add_consumer(f,name),0);
 KUNIT_EXPECT_PTR_EQ(test,f->consumer->dev.driver,NULL);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->consumer),NULL);KUNIT_EXPECT_EQ(test,f->provider.users,0);
}
static void zero_width(struct kunit *test){reject(test,"zero-width");}
static void overflow(struct kunit *test){reject(test,"overflow");}
static void bad_length(struct kunit *test){reject(test,"bad-length");}
static void missing_table(struct kunit *test){reject(test,"no-table");}
static void missing_path(struct kunit *test){reject(test,"no-path");}
static void wrong_cells(struct kunit *test){reject(test,"bad-cells");}
static void initial_vote_failure(struct kunit *test)
{
 struct fixture *f=test->priv;WRITE_ONCE(f->fail,1);KUNIT_ASSERT_EQ(test,add_consumer(f,"good"),0);
 KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->consumer),NULL);KUNIT_EXPECT_EQ(test,f->provider.users,0);
 WRITE_ONCE(f->fail,0);KUNIT_ASSERT_EQ(test,device_attach(&f->consumer->dev),1);KUNIT_EXPECT_NOT_NULL(test,platform_get_drvdata(f->consumer));
}
static struct kunit_case cases[]={
 KUNIT_CASE(native_probe),KUNIT_CASE(actual_vote_and_release),KUNIT_CASE(failed_vote_retries),KUNIT_CASE(cancel_pending_with_new_target),
 KUNIT_CASE(pending_remove),KUNIT_CASE(native_rebind),KUNIT_CASE(multilevel_table),KUNIT_CASE(zero_width),KUNIT_CASE(overflow),
 KUNIT_CASE(bad_length),KUNIT_CASE(missing_table),KUNIT_CASE(missing_path),KUNIT_CASE(wrong_cells),KUNIT_CASE(initial_vote_failure),{}
};
static struct kunit_suite suite={.name="avr-ddr-cooling",.init=setup,.exit=teardown,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
