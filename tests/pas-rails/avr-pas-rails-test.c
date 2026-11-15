// SPDX-License-Identifier: GPL-2.0-only
/* Actual rail state machine, regulator calls recorded only in this test TU. */
#include <kunit/test.h>
#include <linux/kthread.h>
#include <linux/regulator/consumer.h>
struct rail_backend { int id, enabled; };
struct event { int rail, op, value; };
static struct {
	struct event events[512];
	unsigned int count;
	int fail_at, fail_at2, error;
	bool positive_load, bad_count;
} backend;
enum { VOLTAGE, LOAD, ENABLE, DISABLE };
static int record(struct regulator *reg, int op, int value)
{
	struct rail_backend *r = (void *)reg;
	unsigned int at = backend.count++;
	if (at < ARRAY_SIZE(backend.events)) backend.events[at] = (struct event){r->id,op,value};
	if (at == backend.fail_at) return backend.error;
	if (at == backend.fail_at2) return -ETIMEDOUT;
	if (op == ENABLE) r->enabled++;
	if (op == DISABLE) r->enabled--;
	if (r->enabled < 0 || r->enabled > 1) backend.bad_count = true;
	return op == LOAD && backend.positive_load ? 4 : 0;
}
static int record_voltage(struct regulator *r, int min, int max) { return record(r,VOLTAGE,min); }
static int record_load(struct regulator *r, int value) { return record(r,LOAD,value); }
static int record_enable(struct regulator *r) { return record(r,ENABLE,1); }
static int record_disable(struct regulator *r) { return record(r,DISABLE,0); }
#define regulator_set_voltage record_voltage
#define regulator_set_load record_load
#define regulator_enable record_enable
#define regulator_disable record_disable
#include "../../ports/qcom-pas-alor/avr_pas_rails.h"
struct fixture { struct avr_pas_rails rails; struct rail_backend regs[2]; };
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
	int i;
	if (!f) return -ENOMEM;
	memset(&backend,0,sizeof(backend)); backend.fail_at = -1; backend.fail_at2 = -1; backend.error = -EIO;
	mutex_init(&f->rails.lock);
	for (i=0;i<2;i++) {
		f->regs[i].id=i; f->rails.rail[i].reg=(void *)&f->regs[i];
		f->rails.rail[i].level=64;
	}
	test->priv=f; return 0;
}
static bool empty(struct avr_pas_rails *rails)
{
	int i;
	for (i=0;i<2;i++) if (rails->rail[i].enabled || rails->rail[i].load_set || rails->rail[i].voltage_set) return false;
	return true;
}
static void successful_order_and_idempotent_release(struct kunit *test)
{
	struct fixture *f=test->priv; unsigned int i;
	KUNIT_ASSERT_EQ(test,avr_pas_rails_enable(&f->rails),0);
	KUNIT_EXPECT_EQ(test,backend.count,6U);
	for(i=0;i<6;i++) {
		KUNIT_EXPECT_EQ(test,backend.events[i].rail,(int)(i/3));
		KUNIT_EXPECT_EQ(test,backend.events[i].op,(int)(i%3));
	}
	KUNIT_EXPECT_EQ(test,backend.events[0].value,64);
	KUNIT_EXPECT_EQ(test,backend.events[3].value,64);
	KUNIT_EXPECT_EQ(test,backend.events[1].value,0);
	KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),0);
	KUNIT_EXPECT_EQ(test,backend.count,12U);
	KUNIT_EXPECT_EQ(test,backend.events[6].rail,1);
	KUNIT_EXPECT_EQ(test,backend.events[6].op,DISABLE);
	KUNIT_EXPECT_EQ(test,backend.events[7].op,LOAD);
	KUNIT_EXPECT_EQ(test,backend.events[8].op,VOLTAGE);
	KUNIT_EXPECT_EQ(test,backend.events[9].rail,0);
	KUNIT_EXPECT_TRUE(test,empty(&f->rails));
	KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),0);
	KUNIT_EXPECT_EQ(test,backend.count,12U);
	KUNIT_EXPECT_FALSE(test,backend.bad_count);
}
static void every_forward_failure_rolls_back(struct kunit *test)
{
	struct fixture *f=test->priv; int i;
	for(i=0;i<6;i++) {
		backend.count=0; backend.fail_at=i;
		KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),-EIO);
		KUNIT_EXPECT_TRUE(test,empty(&f->rails));
		KUNIT_EXPECT_EQ(test,f->regs[0].enabled,0); KUNIT_EXPECT_EQ(test,f->regs[1].enabled,0);
		KUNIT_EXPECT_FALSE(test,backend.bad_count);
		KUNIT_EXPECT_EQ(test,f->rails.cleanup_error,0);
	}
}
static void every_release_failure_is_retryable(struct kunit *test)
{
	struct fixture *f=test->priv; int i;
	for(i=0;i<6;i++) {
		backend.count=0; backend.fail_at=-1;
		KUNIT_ASSERT_EQ(test,avr_pas_rails_enable(&f->rails),0);
		backend.fail_at=backend.count+i;
		KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),-EIO);
		KUNIT_EXPECT_EQ(test,f->rails.cleanup_error,-EIO);
		KUNIT_EXPECT_FALSE(test,empty(&f->rails));
		KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),-EBUSY);
		backend.fail_at=-1;
		KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),0);
		KUNIT_EXPECT_TRUE(test,empty(&f->rails));
		KUNIT_EXPECT_EQ(test,f->regs[0].enabled,0); KUNIT_EXPECT_EQ(test,f->regs[1].enabled,0);
		KUNIT_EXPECT_FALSE(test,backend.bad_count);
	}
}
static void primary_error_survives_failed_rollback(struct kunit *test)
{
	struct fixture *f=test->priv;
	backend.fail_at=5; backend.error=-EPROBE_DEFER; backend.fail_at2=6;
	KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),-EPROBE_DEFER);
	KUNIT_EXPECT_EQ(test,f->rails.cleanup_error,-ETIMEDOUT);
	KUNIT_EXPECT_TRUE(test,f->rails.rail[0].enabled);
	KUNIT_EXPECT_FALSE(test,f->rails.rail[1].enabled);
	KUNIT_EXPECT_TRUE(test,f->rails.rail[1].load_set);
	KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),-EBUSY);
	backend.fail_at=backend.fail_at2=-1;
	KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),0);
	KUNIT_EXPECT_TRUE(test,empty(&f->rails));
	KUNIT_EXPECT_FALSE(test,backend.bad_count);
}
static void no_double_enable(struct kunit *test)
{
	struct fixture *f=test->priv;
	KUNIT_ASSERT_EQ(test,avr_pas_rails_enable(&f->rails),0);
	KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),-EBUSY);
	KUNIT_EXPECT_EQ(test,backend.count,6U);
	KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),0);
}
static void positive_load_return_is_success(struct kunit *test)
{
	struct fixture *f=test->priv; backend.positive_load=true;
	KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),0);
	KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),0);
	KUNIT_EXPECT_TRUE(test,empty(&f->rails));
}
static void invalid_inputs_have_no_side_effects(struct kunit *test)
{
	struct fixture *f=test->priv;
	f->rails.rail[1].reg=ERR_PTR(-EPROBE_DEFER);
	KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),-EINVAL);
	f->rails.rail[1].reg=(void *)&f->regs[1]; f->rails.rail[1].level=-1;
	KUNIT_EXPECT_EQ(test,avr_pas_rails_enable(&f->rails),-EINVAL);
	KUNIT_EXPECT_EQ(test,backend.count,0U);
}
static int toggle_worker(void *priv)
{
	struct avr_pas_rails *rails=priv; int i;
	for(i=0;i<200;i++) { avr_pas_rails_enable(rails); cond_resched(); avr_pas_rails_release(rails); }
	return 0;
}
static void concurrent_enable_and_release(struct kunit *test)
{
	struct fixture *f=test->priv;
	struct task_struct *a=kthread_run(toggle_worker,&f->rails,"avr_rails_a"), *b;
	KUNIT_ASSERT_FALSE(test,IS_ERR(a));
	b=kthread_run(toggle_worker,&f->rails,"avr_rails_b");
	if (IS_ERR(b)) {kthread_stop(a); KUNIT_FAIL(test,"cannot start second worker");return;}
	kthread_stop(a); kthread_stop(b);
	KUNIT_EXPECT_EQ(test,avr_pas_rails_release(&f->rails),0);
	KUNIT_EXPECT_TRUE(test,empty(&f->rails));
	KUNIT_EXPECT_FALSE(test,backend.bad_count);
	KUNIT_EXPECT_EQ(test,f->regs[0].enabled,0); KUNIT_EXPECT_EQ(test,f->regs[1].enabled,0);
}
static struct kunit_case cases[]={
 KUNIT_CASE(successful_order_and_idempotent_release),KUNIT_CASE(every_forward_failure_rolls_back),
 KUNIT_CASE(every_release_failure_is_retryable),KUNIT_CASE(no_double_enable),KUNIT_CASE(primary_error_survives_failed_rollback),
 KUNIT_CASE(positive_load_return_is_success),KUNIT_CASE(invalid_inputs_have_no_side_effects),
 KUNIT_CASE(concurrent_enable_and_release),{}
};
static struct kunit_suite suite={.name="avr-pas-rails",.init=setup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");

MODULE_DESCRIPTION("AVR PAS counted rail state machine tests");
