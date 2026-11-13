// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>

struct proxy_test {
 struct mutex held;
 struct completion trying;
 struct completion gp_done;
 bool release;
};
static void wait_for_stop(void)
{
 while (!kthread_should_stop())
  schedule_timeout_interruptible(1);
}
static int blocked_donor(void *arg)
{
 struct proxy_test *f=arg;
 complete(&f->trying);
 mutex_lock(&f->held);
 mutex_unlock(&f->held);
 wait_for_stop();
 return 0;
}
static int wait_gp(void *arg)
{
 struct proxy_test *f=arg;
 synchronize_rcu_tasks();
 complete(&f->gp_done);
 wait_for_stop();
 return 0;
}
static void blocked_proxy_donor_is_quiescent(struct kunit *test)
{
 struct proxy_test f;
 struct task_struct *donor,*gp;
 unsigned long done=0,blocked=0;
 mutex_init(&f.held);
 init_completion(&f.trying);
 init_completion(&f.gp_done);
 mutex_lock(&f.held);
 donor=kthread_run(blocked_donor,&f,"avr_rcu_donor");
 if (IS_ERR(donor)) {
  mutex_unlock(&f.held);
  KUNIT_FAIL(test,"cannot create donor");
  return;
 }
 if (wait_for_completion_timeout(&f.trying,5*HZ)) {
  for (int i=0;i<1000;i++) {
   if (READ_ONCE(donor->is_blocked) && !READ_ONCE(donor->on_cpu)) {
    blocked=1;
    break;
   }
   msleep(1);
  }
 }
 gp=kthread_run(wait_gp,&f,"avr_rcu_wait_gp");
 if (!IS_ERR(gp))
  done=wait_for_completion_timeout(&f.gp_done,10*HZ);
 /* Always release and join before reporting failure: the old bug would
  * otherwise leave a kernel thread referencing a dead test stack. */
 mutex_unlock(&f.held);
 kthread_stop(donor);
 if (!IS_ERR(gp)) kthread_stop(gp);
 KUNIT_EXPECT_TRUE(test,blocked);
 KUNIT_EXPECT_FALSE(test,IS_ERR(gp));
 KUNIT_EXPECT_TRUE(test,done);
 mutex_destroy(&f.held);
}
static int running_reader(void *arg)
{
 struct proxy_test *f=arg;
 complete(&f->trying);
 /* Preemption is allowed; no voluntary quiescent state until released. */
 while (!READ_ONCE(f->release))
  cpu_relax();
 wait_for_stop();
 return 0;
}
static void runnable_task_remains_holdout(struct kunit *test)
{
 struct proxy_test f={0};
 struct task_struct *reader,*gp;
 unsigned long premature=0,finished=0,started;
 init_completion(&f.trying);
 init_completion(&f.gp_done);
 reader=kthread_run(running_reader,&f,"avr_rcu_reader");
 KUNIT_ASSERT_FALSE(test,IS_ERR(reader));
 started=wait_for_completion_timeout(&f.trying,5*HZ);
 gp=kthread_run(wait_gp,&f,"avr_rcu_reader_gp");
 if (!IS_ERR(gp))
  premature=wait_for_completion_timeout(&f.gp_done,msecs_to_jiffies(200));
 WRITE_ONCE(f.release,true);
 kthread_stop(reader);
 if (!IS_ERR(gp)) {
  if (!premature) finished=wait_for_completion_timeout(&f.gp_done,10*HZ);
  kthread_stop(gp);
 }
 KUNIT_EXPECT_TRUE(test,started);
 KUNIT_EXPECT_FALSE(test,IS_ERR(gp));
 KUNIT_EXPECT_FALSE(test,premature);
 KUNIT_EXPECT_TRUE(test,finished);
}
static struct kunit_case proxy_cases[]={KUNIT_CASE(blocked_proxy_donor_is_quiescent),KUNIT_CASE(runnable_task_remains_holdout),{}};
static struct kunit_suite proxy_suite={.name="avr_rcu_proxy",.test_cases=proxy_cases};
kunit_test_suite(proxy_suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Tasks RCU progress with a mutex-blocked proxy donor");
