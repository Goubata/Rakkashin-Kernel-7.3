// SPDX-License-Identifier: GPL-2.0-only
/* Production ownership helper with recording callbacks only in this test. */
#include <kunit/test.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include "../../ports/qcom-pas-alor/avr_pas_proxy.h"
struct fixture {
	struct avr_pas_proxy proxy;
	int count[AVR_PROXY_COUNT];
	int fail_acquire, fail_release;
	bool bad_count;
	int trace[4096];
	unsigned int traces;
};
static void record(struct fixture *f,int step,bool release)
{
	if(f->traces<ARRAY_SIZE(f->trace))f->trace[f->traces]=release ? -(step+1) : step+1;
	f->traces++;
}
static int acquire(void *ctx,unsigned int step)
{
	struct fixture *f=ctx;record(f,step,false);
	if(f->fail_acquire==step && step!=AVR_PROXY_ARC)return -EIO;
	if(++f->count[step]!=1)f->bad_count=true;
	return f->fail_acquire==step ? -EIO : 0;
}
static int release(void *ctx,unsigned int step)
{
	struct fixture *f=ctx;record(f,step,true);
	if(f->fail_release==step)return -ETIMEDOUT;
	if(--f->count[step]!=0)f->bad_count=true;
	return 0;
}
static const struct avr_pas_proxy_ops ops={.acquire=acquire,.release=release};
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
	if(!f)return -ENOMEM;
	f->fail_acquire=f->fail_release=-1;
	mutex_init(&f->proxy.lock);f->proxy.ops=&ops;f->proxy.ctx=f;
	test->priv=f;return 0;
}
static void balanced(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	KUNIT_EXPECT_EQ(test,f->proxy.owned,0UL);
	KUNIT_EXPECT_FALSE(test,f->bad_count);
	for(i=0;i<AVR_PROXY_COUNT;i++)KUNIT_EXPECT_EQ(test,f->count[i],0);
}
static void ordered_acquire_release(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_acquire(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,f->proxy.owned,BIT(AVR_PROXY_COUNT)-1);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);
	for(i=0;i<AVR_PROXY_COUNT;i++) {
		KUNIT_EXPECT_EQ(test,f->trace[i],i+1);
		KUNIT_EXPECT_EQ(test,f->trace[i+AVR_PROXY_COUNT],-(AVR_PROXY_COUNT-i));
	}
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,f->traces,12U);balanced(test);
}
static void every_acquire_failure_unwinds_only_owned(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	for(i=0;i<AVR_PROXY_COUNT;i++) {
		f->fail_acquire=i;
		KUNIT_ASSERT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_acquire(&f->proxy),-EIO);
		KUNIT_EXPECT_EQ(test,f->proxy.cleanup_error,0);
		balanced(test);
	}
}
static void every_release_failure_retains_ownership(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	for(i=0;i<AVR_PROXY_COUNT;i++) {
		KUNIT_ASSERT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
		KUNIT_ASSERT_EQ(test,avr_pas_proxy_acquire(&f->proxy),0);
		f->fail_release=i;
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),-ETIMEDOUT);
		KUNIT_EXPECT_EQ(test,f->proxy.owned,BIT(i+1)-1);
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_begin(&f->proxy),-EBUSY);
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_acquire(&f->proxy),-EBUSY);
		f->fail_release=-1;
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);
		balanced(test);
	}
}
static void early_handover_waits_for_authentication(struct kunit *test)
{
	struct fixture *f=test->priv;
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_handover(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,f->traces,0U);
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_acquire(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_handover(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,f->traces,6U);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_allow_handover(&f->proxy),0);
	balanced(test);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_handover(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,f->traces,12U);
}
static void late_handover_releases_once(struct kunit *test)
{
	struct fixture *f=test->priv;
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_acquire(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_allow_handover(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,f->traces,6U);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_handover(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,f->traces,12U);balanced(test);
}
static void dual_failure_preserves_primary_and_cleanup(struct kunit *test)
{
	struct fixture *f=test->priv;
	f->fail_acquire=AVR_PROXY_AGGRE;f->fail_release=AVR_PROXY_XO;
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_acquire(&f->proxy),-EIO);
	KUNIT_EXPECT_EQ(test,f->proxy.cleanup_error,-ETIMEDOUT);
	KUNIT_EXPECT_EQ(test,f->proxy.owned,BIT(AVR_PROXY_XO+1)-1);
	f->fail_release=-1;
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);balanced(test);
}
static void failed_handover_can_be_retried_by_stop(struct kunit *test)
{
	struct fixture *f=test->priv;
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
	KUNIT_ASSERT_EQ(test,avr_pas_proxy_acquire(&f->proxy),0);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_allow_handover(&f->proxy),0);
	f->fail_release=AVR_PROXY_XO;
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_handover(&f->proxy),-ETIMEDOUT);
	KUNIT_EXPECT_EQ(test,f->proxy.owned,BIT(AVR_PROXY_XO+1)-1);
	f->fail_release=-1;
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);balanced(test);
	KUNIT_EXPECT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
	KUNIT_EXPECT_FALSE(test,f->proxy.handover_pending);
}
static int handover_worker(void *ctx)
{
	struct avr_pas_proxy *proxy=ctx;
	while(!kthread_should_stop()){avr_pas_proxy_handover(proxy);cond_resched();}
	return 0;
}
static void concurrent_handover_and_cycles(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	struct task_struct *thread=kthread_run(handover_worker,&f->proxy,"avr_proxy_handover");
	KUNIT_ASSERT_FALSE(test,IS_ERR(thread));
	for(i=0;i<200;i++) {
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_begin(&f->proxy),0);
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_acquire(&f->proxy),0);
		cond_resched();
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_allow_handover(&f->proxy),0);
		KUNIT_EXPECT_EQ(test,avr_pas_proxy_release(&f->proxy),0);
	}
	kthread_stop(thread);balanced(test);
}
static struct kunit_case cases[]={
 KUNIT_CASE(ordered_acquire_release),KUNIT_CASE(every_acquire_failure_unwinds_only_owned),
 KUNIT_CASE(every_release_failure_retains_ownership),KUNIT_CASE(early_handover_waits_for_authentication),
 KUNIT_CASE(late_handover_releases_once),KUNIT_CASE(dual_failure_preserves_primary_and_cleanup),
 KUNIT_CASE(failed_handover_can_be_retried_by_stop),KUNIT_CASE(concurrent_handover_and_cycles),{}
};
static struct kunit_suite suite={.name="avr-pas-proxy",.init=setup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_DESCRIPTION("AVR PAS proxy ownership and handover tests");
MODULE_LICENSE("GPL");
