// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#define AVR_SYSSTATS_TEST
#include "../../ports/msm-sysstats/msm_sysstats.c"

static u64 sample(pid_t pid) {return 4096 + pid;}
static void callback_value(struct kunit *test)
{
 sysstats_register_kgsl_stats_cb(sample);
 KUNIT_EXPECT_EQ(test,sysstats_kgsl_get_stats(7),(u64)4103);
 sysstats_unregister_kgsl_stats_cb();
 KUNIT_EXPECT_EQ(test,sysstats_kgsl_get_stats(7),(u64)0);
}
static void null_callback(struct kunit *test)
{
 sysstats_register_kgsl_stats_cb(NULL);
 KUNIT_EXPECT_EQ(test,sysstats_kgsl_get_stats(-1),(u64)0);
 sysstats_unregister_kgsl_stats_cb();
}
static void memory_counters(struct kunit *test)
{
 struct sysstats_mem stats={};
 struct sysinfo si;
 void *allocation=vmalloc(128*PAGE_SIZE);
 KUNIT_ASSERT_NOT_NULL(test,allocation);
 sysstats_build(&stats);
 si_meminfo(&si);
 KUNIT_EXPECT_EQ(test,stats.memtotal,(u64)si.totalram << (PAGE_SHIFT-10));
 KUNIT_EXPECT_GE(test,stats.vmalloc_total,(u64)128*PAGE_SIZE/1024);
 vfree(allocation);
}
static DECLARE_COMPLETION(entered);
static DECLARE_COMPLETION(allow_return);
static DECLARE_COMPLETION(reader_done);
static DECLARE_COMPLETION(unregister_started);
static DECLARE_COMPLETION(unregister_done);
static u64 sleeping(pid_t pid)
{
 complete(&entered);
 wait_for_completion(&allow_return);
 return 123;
}
static int reader(void *unused)
{
 sysstats_kgsl_get_stats(1);
 complete(&reader_done);
 return 0;
}
static int unregister_worker(void *unused)
{
 complete(&unregister_started);
 sysstats_unregister_kgsl_stats_cb();
 complete(&unregister_done);
 return 0;
}
static void sleeping_unregister(struct kunit *test)
{
 struct task_struct *r,*u;
 unsigned long ready,started,read_end,unreg_end;
 bool early=false;
 reinit_completion(&entered);reinit_completion(&allow_return);
 reinit_completion(&reader_done);reinit_completion(&unregister_started);
 reinit_completion(&unregister_done);
 sysstats_register_kgsl_stats_cb(sleeping);
 r=kthread_run(reader,NULL,"sysstats-reader");
 if (IS_ERR(r)) {
  sysstats_unregister_kgsl_stats_cb();
  KUNIT_FAIL(test,"reader creation");return;
 }
 ready=wait_for_completion_timeout(&entered,5*HZ);
 u=kthread_run(unregister_worker,NULL,"sysstats-unreg");
 if (IS_ERR(u)) {
  complete(&allow_return);wait_for_completion(&reader_done);
  sysstats_unregister_kgsl_stats_cb();
  KUNIT_FAIL(test,"unregister worker creation");return;
 }
 started=wait_for_completion_timeout(&unregister_started,5*HZ);
 if (ready && started) {
  msleep(30);
  early=completion_done(&unregister_done);
 }
 complete(&allow_return);
 read_end=wait_for_completion_timeout(&reader_done,5*HZ);
 unreg_end=wait_for_completion_timeout(&unregister_done,5*HZ);
 KUNIT_EXPECT_NE(test,ready,0UL);
 KUNIT_EXPECT_NE(test,started,0UL);
 KUNIT_EXPECT_FALSE(test,early);
 KUNIT_EXPECT_NE(test,read_end,0UL);
 KUNIT_EXPECT_NE(test,unreg_end,0UL);
 KUNIT_EXPECT_EQ(test,sysstats_kgsl_get_stats(1),(u64)0);
}
static struct kunit_case cases[]={KUNIT_CASE(callback_value),KUNIT_CASE(null_callback),
 KUNIT_CASE(memory_counters),KUNIT_CASE(sleeping_unregister),{}};
static struct kunit_suite suite={.name="avr-sysstats",.test_cases=cases};
kunit_test_suite(suite);
