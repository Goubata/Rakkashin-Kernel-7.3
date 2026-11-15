// SPDX-License-Identifier: GPL-2.0-only
/* Real exported relay/notifier and sensor class APIs; no physical device IO. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mmi_relay.h>
#include <linux/sensors.h>
struct fixture;
struct subscriber {struct notifier_block nb;struct fixture *f;notifier_t type;notifier_d dev;bool registered;};
struct fixture {
	struct platform_device *pdev;
	struct subscriber sub[3];
	atomic_t calls;
	bool block,metadata_register,metadata_started,timed_out;
	unsigned long last_value;void *last_data;int nested_result,unregister_result;
	struct completion entered,allow,remove_started,remove_done,client_started,client_done;
};
static int add(struct subscriber *sub)
{
	int ret=relay_register_action(sub->type,sub->dev,&sub->nb);if(!ret)sub->registered=true;return ret;
}
static int callback(struct notifier_block *nb,unsigned long value,void *data)
{
	struct subscriber *sub=container_of(nb,struct subscriber,nb);struct fixture *f=sub->f;
	if(sub==&f->sub[2]) {
		if(f->metadata_register && !f->metadata_started){f->metadata_started=true;f->nested_result=add(&f->sub[1]);}
		return NOTIFY_OK;
	}
	atomic_inc(&f->calls);f->last_value=value;f->last_data=data;
	if(f->block && sub==&f->sub[0]){complete(&f->entered);if(!wait_for_completion_timeout(&f->allow,5*HZ))f->timed_out=true;}
	return NOTIFY_OK;
}
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int i;
	if(!f)return -ENOMEM;
	init_completion(&f->entered);init_completion(&f->allow);init_completion(&f->remove_started);init_completion(&f->remove_done);init_completion(&f->client_started);init_completion(&f->client_done);
	for(i=0;i<3;i++){f->sub[i].nb.notifier_call=callback;f->sub[i].f=f;}
	f->sub[0].type=BLOCKING;f->sub[0].dev=FPS;
	f->sub[1].type=ATOMIC;f->sub[1].dev=LCD;
	f->sub[2].type=BLOCKING;f->sub[2].dev=MMI_RELAY;
	f->pdev=platform_device_register_simple("mmi_relay",PLATFORM_DEVID_AUTO,NULL,0);
	if(IS_ERR(f->pdev))return PTR_ERR(f->pdev);
	if(!f->pdev->dev.driver){platform_device_unregister(f->pdev);return -ENODEV;}
	test->priv=f;return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f=test->priv;int i;complete_all(&f->allow);
	for(i=0;i<3;i++)if(f->sub[i].registered)relay_unregister_action(f->sub[i].type,f->sub[i].dev,&f->sub[i].nb);
	if(f->pdev)platform_device_unregister(f->pdev);
}
static void blocking_payload_roundtrip(struct kunit *test)
{
	struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,add(&f->sub[0]),0);
	KUNIT_EXPECT_EQ(test,relay_notifier_fire(BLOCKING,FPS,144,f),NOTIFY_OK);
	KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),1);KUNIT_EXPECT_EQ(test,f->last_value,144UL);KUNIT_EXPECT_PTR_EQ(test,f->last_data,f);
}
static void atomic_dispatch_in_atomic_context(struct kunit *test)
{
	struct fixture *f=test->priv;int ret;KUNIT_ASSERT_EQ(test,add(&f->sub[1]),0);
	preempt_disable();ret=relay_notifier_fire(ATOMIC,LCD,1,f);preempt_enable();
	KUNIT_EXPECT_EQ(test,ret,NOTIFY_OK);KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),1);
}
static void device_and_type_namespaces_are_independent(struct kunit *test)
{
	struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,add(&f->sub[0]),0);
	KUNIT_EXPECT_EQ(test,relay_notifier_fire(BLOCKING,LCD,1,f),NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test,relay_notifier_fire(ATOMIC,FPS,1,f),NOTIFY_DONE);
	KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),0);
}
static void invalid_arguments_do_not_index_arrays(struct kunit *test)
{
	struct fixture *f=test->priv;
	KUNIT_EXPECT_EQ(test,relay_notifier_fire((notifier_t)-1,FPS,0,NULL),-EPERM);
	KUNIT_EXPECT_EQ(test,relay_notifier_fire(BLOCKING,(notifier_d)-1,0,NULL),-EPERM);
	KUNIT_EXPECT_EQ(test,relay_register_action(NUM_TYPES,FPS,&f->sub[0].nb),-EPERM);
	KUNIT_EXPECT_EQ(test,relay_unregister_action(BLOCKING,NUM_DEVS,&f->sub[0].nb),-EPERM);
	KUNIT_EXPECT_EQ(test,relay_register_action(BLOCKING,FPS,NULL),-EINVAL);
}
static void metadata_callback_can_register_other_notifier_type(struct kunit *test)
{
	struct fixture *f=test->priv;f->metadata_register=true;
	KUNIT_ASSERT_EQ(test,add(&f->sub[2]),0);KUNIT_EXPECT_TRUE(test,f->metadata_started);KUNIT_EXPECT_EQ(test,f->nested_result,0);
	KUNIT_EXPECT_EQ(test,relay_notifier_fire(ATOMIC,LCD,7,f),NOTIFY_OK);KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),1);
}
static int sender(void *arg)
{
	struct fixture *f=arg;relay_notifier_fire(BLOCKING,FPS,144,f);
	while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static int remove_provider(void *arg)
{
	struct fixture *f=arg;complete(&f->remove_started);platform_device_unregister(f->pdev);complete(&f->remove_done);
	while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static int remove_client(void *arg)
{
	struct fixture *f=arg;complete(&f->client_started);f->unregister_result=relay_unregister_action(BLOCKING,FPS,&f->sub[0].nb);complete(&f->client_done);
	while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static void blocked_drain(struct kunit *test,bool provider)
{
	struct fixture *f=test->priv;struct task_struct *send,*remove=NULL,*client=NULL;int i,ret=0;
	f->block=true;KUNIT_ASSERT_EQ(test,add(&f->sub[0]),0);
	send=kthread_run(sender,f,"avr_relay_sender");KUNIT_ASSERT_FALSE(test,IS_ERR(send));
	if(!wait_for_completion_timeout(&f->entered,2*HZ)){KUNIT_FAIL(test,"callback did not enter");goto out;}
	if(provider) {
		remove=kthread_run(remove_provider,f,"avr_relay_remove");if(IS_ERR(remove)){remove=NULL;KUNIT_FAIL(test,"remove thread failed");goto out;}
		if(!wait_for_completion_timeout(&f->remove_started,2*HZ)){KUNIT_FAIL(test,"remove did not start");goto out;}
		for(i=0;i<100;i++){ret=relay_notifier_fire(ATOMIC,LCD,0,NULL);if(ret==-EINVAL)break;usleep_range(1000,2000);}
		KUNIT_EXPECT_EQ(test,ret,-EINVAL);
		if(ret!=-EINVAL)goto out;
		KUNIT_EXPECT_FALSE(test,completion_done(&f->remove_done));
	}
	client=kthread_run(remove_client,f,"avr_relay_client");if(IS_ERR(client)){client=NULL;KUNIT_FAIL(test,"client thread failed");goto out;}
	if(!wait_for_completion_timeout(&f->client_started,2*HZ)){KUNIT_FAIL(test,"client did not start");goto out;}
	KUNIT_EXPECT_EQ(test,wait_for_completion_timeout(&f->client_done,msecs_to_jiffies(50)),0UL);
out:
	complete_all(&f->allow);kthread_stop(send);
	if(client){kthread_stop(client);f->sub[0].registered=false;KUNIT_EXPECT_EQ(test,f->unregister_result,provider?-EINVAL:0);}
	if(remove){kthread_stop(remove);f->pdev=NULL;f->sub[0].registered=false;KUNIT_EXPECT_TRUE(test,completion_done(&f->remove_done));}
	KUNIT_EXPECT_FALSE(test,f->timed_out);
	if(provider && remove) {
		KUNIT_EXPECT_EQ(test,relay_notifier_fire(BLOCKING,FPS,0,NULL),-EINVAL);
		f->pdev=platform_device_register_simple("mmi_relay",PLATFORM_DEVID_AUTO,NULL,0);
		if(IS_ERR(f->pdev)){f->pdev=NULL;KUNIT_FAIL(test,"provider re-probe failed");return;}
		KUNIT_ASSERT_NOT_NULL(test,f->pdev->dev.driver);f->block=false;
		KUNIT_ASSERT_EQ(test,add(&f->sub[0]),0);KUNIT_EXPECT_EQ(test,relay_notifier_fire(BLOCKING,FPS,1,f),NOTIFY_OK);
	}
}
static void client_unregister_waits_for_callback(struct kunit *test){blocked_drain(test,false);}
static void provider_and_absent_unregister_drain_callback(struct kunit *test){blocked_drain(test,true);}
static void actual_sensor_class_registration(struct kunit *test)
{
	struct fixture *f=test->priv;struct sensors_classdev sensor={.name="avr_touch_gesture_test",.vendor="Motorola",.max_range="1",.resolution="1",.sensor_power="0"};
	KUNIT_ASSERT_EQ(test,sensors_classdev_register(&f->pdev->dev,&sensor),0);
	KUNIT_EXPECT_TRUE(test,device_is_registered(sensor.dev));KUNIT_EXPECT_STREQ(test,sensor.dev->class->name,"sensors");
	KUNIT_EXPECT_PTR_EQ(test,dev_get_drvdata(sensor.dev),&sensor);sensors_classdev_unregister(&sensor);
	KUNIT_EXPECT_PTR_EQ(test,sensor.dev,NULL);sensors_classdev_unregister(&sensor);
}
static void invalid_sensor_registration_is_rejected(struct kunit *test)
{
	struct fixture *f=test->priv;struct sensors_classdev sensor={};
	KUNIT_EXPECT_EQ(test,sensors_classdev_register(&f->pdev->dev,NULL),-EINVAL);
	KUNIT_EXPECT_EQ(test,sensors_classdev_register(&f->pdev->dev,&sensor),-EINVAL);
	sensor.name="";KUNIT_EXPECT_EQ(test,sensors_classdev_register(&f->pdev->dev,&sensor),-EINVAL);
	sensors_classdev_unregister(NULL);
}
static struct kunit_case cases[]={KUNIT_CASE(blocking_payload_roundtrip),KUNIT_CASE(atomic_dispatch_in_atomic_context),KUNIT_CASE(device_and_type_namespaces_are_independent),KUNIT_CASE(invalid_arguments_do_not_index_arrays),KUNIT_CASE(metadata_callback_can_register_other_notifier_type),KUNIT_CASE(client_unregister_waits_for_callback),KUNIT_CASE(provider_and_absent_unregister_drain_callback),KUNIT_CASE(actual_sensor_class_registration),KUNIT_CASE(invalid_sensor_registration_is_rejected),{}};
static struct kunit_suite suite={.name="avr-moto-touch-deps",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("Actual Motorola relay and sensor class API tests");
