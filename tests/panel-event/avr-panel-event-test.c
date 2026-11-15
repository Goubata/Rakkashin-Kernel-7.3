// SPDX-License-Identifier: GPL-2.0-only
/* Real exported notifier API; pointer-only panels, no physical display access. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/soc/qcom/panel_event_notifier.h>
struct fixture {
	struct drm_panel panel[2];
	struct panel_event_notification notification;
	void *cookies[PANEL_EVENT_NOTIFIER_CLIENT_MAX];
	atomic_t calls;int mode;bool callback_timeout;
	struct completion entered,allow,writer_started,writer_done;
};
static void callback(enum panel_event_notifier_tag tag,struct panel_event_notification *event,void *data)
{
	struct fixture *f=data;atomic_inc(&f->calls);
	if(f->mode==1){complete(&f->entered);if(!wait_for_completion_timeout(&f->allow,5*HZ))f->callback_timeout=true;}
	if(f->mode==2 && !f->cookies[1])f->cookies[1]=panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,1,&f->panel[0],callback,f);
}
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);if(!f)return -ENOMEM;
	f->notification=(struct panel_event_notification){.panel=&f->panel[0],.notif_type=DRM_PANEL_EVENT_UNBLANK};
	init_completion(&f->entered);init_completion(&f->allow);init_completion(&f->writer_started);init_completion(&f->writer_done);
	atomic_set(&f->calls,0);test->priv=f;return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	complete_all(&f->allow);
	for(i=0;i<ARRAY_SIZE(f->cookies);i++)if(!IS_ERR_OR_NULL(f->cookies[i]))panel_event_notifier_unregister(f->cookies[i]);
}
static void *subscribe(struct fixture *f,int client){return panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,client,&f->panel[0],callback,f);}
static void invalid_subscriptions_rejected(struct kunit *test)
{
	struct fixture *f=test->priv;void *cookie;
	cookie=panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_NONE,0,&f->panel[0],callback,f);KUNIT_EXPECT_PTR_EQ(test,cookie,ERR_PTR(-EINVAL));
	cookie=panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_MAX,0,&f->panel[0],callback,f);KUNIT_EXPECT_PTR_EQ(test,cookie,ERR_PTR(-EINVAL));
	KUNIT_EXPECT_PTR_EQ(test,subscribe(f,-1),ERR_PTR(-EINVAL));KUNIT_EXPECT_PTR_EQ(test,subscribe(f,PANEL_EVENT_NOTIFIER_CLIENT_MAX),ERR_PTR(-EINVAL));
	cookie=panel_event_notifier_register(PANEL_EVENT_NOTIFICATION_PRIMARY,0,&f->panel[0],NULL,f);KUNIT_EXPECT_PTR_EQ(test,cookie,ERR_PTR(-EINVAL));
	panel_event_notifier_unregister(NULL);
}
static void panel_and_tag_routing(struct kunit *test)
{
	struct fixture *f=test->priv;f->cookies[0]=subscribe(f,0);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[0]));
	panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_SECONDARY,&f->notification);
	f->notification.panel=&f->panel[1];panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);
	KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),0);
	f->notification.panel=&f->panel[0];panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);
	KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),1);
}
static void all_five_stock_clients_receive_events(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	KUNIT_ASSERT_EQ(test,PANEL_EVENT_NOTIFIER_CLIENT_MAX,5);
	for(i=0;i<PANEL_EVENT_NOTIFIER_CLIENT_MAX;i++){f->cookies[i]=subscribe(f,i);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[i]));}
	panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);
	KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),5);
}
static void duplicate_subscription_rejected(struct kunit *test)
{
	struct fixture *f=test->priv;f->cookies[0]=subscribe(f,0);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[0]));
	KUNIT_EXPECT_PTR_EQ(test,subscribe(f,0),ERR_PTR(-EEXIST));
}
static void unregister_stops_future_delivery(struct kunit *test)
{
	struct fixture *f=test->priv;f->cookies[0]=subscribe(f,0);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[0]));
	panel_event_notifier_unregister(f->cookies[0]);f->cookies[0]=NULL;
	panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),0);
}
static void callback_can_register_another_client(struct kunit *test)
{
	struct fixture *f=test->priv;f->mode=2;f->cookies[0]=subscribe(f,0);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[0]));
	/* Callback registration is allowed; unregistering from a callback is not. */
	panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);
	KUNIT_EXPECT_FALSE(test,IS_ERR_OR_NULL(f->cookies[1]));f->mode=0;
}
static int sender(void *arg)
{
	struct fixture *f=arg;panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static int remover(void *arg)
{
	struct fixture *f=arg;complete(&f->writer_started);panel_event_notifier_unregister(f->cookies[0]);complete(&f->writer_done);while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static void unregister_waits_for_running_callback(struct kunit *test)
{
	struct fixture *f=test->priv;struct task_struct *send_task,*remove_task=NULL;bool early=false;
	f->mode=1;f->cookies[0]=subscribe(f,0);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[0]));
	send_task=kthread_run(sender,f,"avr_panel_sender");KUNIT_ASSERT_FALSE(test,IS_ERR(send_task));
	if(!wait_for_completion_timeout(&f->entered,2*HZ)){KUNIT_FAIL(test,"sender did not enter callback");goto out;}
	remove_task=kthread_run(remover,f,"avr_panel_remove");if(IS_ERR(remove_task)){remove_task=NULL;KUNIT_FAIL(test,"could not start remover");goto out;}
	if(!wait_for_completion_timeout(&f->writer_started,2*HZ)){KUNIT_FAIL(test,"remover did not start");goto out;}
	early=wait_for_completion_timeout(&f->writer_done,msecs_to_jiffies(50))!=0;
	KUNIT_EXPECT_FALSE(test,early);
	if(!early)KUNIT_EXPECT_PTR_EQ(test,subscribe(f,0),ERR_PTR(-EBUSY));
out:
	complete_all(&f->allow);kthread_stop(send_task);
	if(remove_task){kthread_stop(remove_task);f->cookies[0]=NULL;KUNIT_EXPECT_TRUE(test,completion_done(&f->writer_done));}
	KUNIT_EXPECT_FALSE(test,f->callback_timeout);
	if(remove_task) {
		f->mode=0;f->cookies[0]=subscribe(f,0);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[0]));
		panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);
		KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),2);
	}
}
static void malformed_cookie_and_notification_are_ignored(struct kunit *test)
{
	struct fixture *f=test->priv;f->cookies[0]=subscribe(f,0);KUNIT_ASSERT_FALSE(test,IS_ERR(f->cookies[0]));
	panel_event_notifier_unregister(ERR_PTR(-EINVAL));
	panel_event_notifier_unregister((char *)f->cookies[0]+1);
	panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,NULL);
	panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_NONE,&f->notification);
	panel_event_notification_trigger(PANEL_EVENT_NOTIFICATION_PRIMARY,&f->notification);
	KUNIT_EXPECT_EQ(test,atomic_read(&f->calls),1);
}
static struct kunit_case cases[]={KUNIT_CASE(invalid_subscriptions_rejected),KUNIT_CASE(panel_and_tag_routing),KUNIT_CASE(all_five_stock_clients_receive_events),KUNIT_CASE(duplicate_subscription_rejected),KUNIT_CASE(unregister_stops_future_delivery),KUNIT_CASE(callback_can_register_another_client),KUNIT_CASE(unregister_waits_for_running_callback),KUNIT_CASE(malformed_cookie_and_notification_are_ignored),{}};
static struct kunit_suite suite={.name="avr-panel-event-api",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);MODULE_LICENSE("GPL");

MODULE_DESCRIPTION("Actual panel event notifier API and lifetime tests");
