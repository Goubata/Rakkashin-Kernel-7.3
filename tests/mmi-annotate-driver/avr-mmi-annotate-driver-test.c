// SPDX-License-Identifier: GPL-2.0-only
/* Exact probe/remove/append source and real OF/proc/memremap over reserved QEMU RAM.
 * Only minidump registration primitives are recorded; no real firmware table IO. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <soc/qcom/minidump.h>
static struct {bool live,fail_add,fail_remove,bad;unsigned int adds,removes;struct md_region entry;} md;
static int recorded_add(const struct md_region *entry)
{
	md.adds++;if(md.fail_add)return -EIO;
	if(md.live || !entry->virt_addr || !entry->size)md.bad=true;
	md.entry=*entry;md.live=true;return 3;
}
static int recorded_remove(const struct md_region *entry)
{
	md.removes++;if(!md.live || memcmp(entry,&md.entry,sizeof(*entry)))md.bad=true;
	if(md.fail_remove)return -EIO;
	md.live=false;return 0;
}
#define msm_minidump_add_region recorded_add
#define msm_minidump_remove_region recorded_remove
#pragma push_macro("module_init")
#pragma push_macro("module_exit")
#undef module_init
#undef module_exit
#define module_init(fn) static typeof(fn) *const __maybe_unused init_reference = fn
#define module_exit(fn) static typeof(fn) *const __maybe_unused exit_reference = fn
#include "../../ports/moto-info/mmi_annotate/mmi_annotate.c"
#pragma pop_macro("module_exit")
#pragma pop_macro("module_init")
struct fixture {struct platform_device *pdev;};
static struct platform_device *device_for(const char *path)
{
	struct device_node *node=of_find_node_by_path(path);struct platform_device *pdev;
	if(!node)return NULL;pdev=of_find_device_by_node(node);of_node_put(node);return pdev;
}
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
	struct persist_data_t *ram;
	if(!f)return -ENOMEM;
	memset(&md,0,sizeof(md));
	f->pdev=device_for("/avr-annotate-test");if(!f->pdev)return -ENODEV;
	ram=mmi_annotate_map_ram(0x60000000,0x10000);
	if(!ram){put_device(&f->pdev->dev);return -ENOMEM;}
	memset(ram,0,0x10000);memunmap(ram);
	test->priv=f;return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f=test->priv;md.fail_remove=false;
	if(platform_get_drvdata(f->pdev))mmi_annotate_remove(f->pdev);
	if(retired_provider){mmi_annotate_release(retired_provider);retired_provider=NULL;}
	put_device(&f->pdev->dev);
}
static void expect_idle(struct kunit *test)
{
	KUNIT_EXPECT_PTR_EQ(test,active_provider,NULL);KUNIT_EXPECT_PTR_EQ(test,mem_data.contents,NULL);
	KUNIT_EXPECT_PTR_EQ(test,persist_data,NULL);KUNIT_EXPECT_FALSE(test,md.live);KUNIT_EXPECT_FALSE(test,md.bad);
}
static void successful_probe_append_and_remove(struct kunit *test)
{
	struct fixture *f=test->priv;struct annotate_provider *ctx;
	KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);ctx=platform_get_drvdata(f->pdev);KUNIT_ASSERT_NOT_NULL(test,ctx);
	KUNIT_EXPECT_TRUE(test,md.live);KUNIT_EXPECT_EQ(test,md.entry.virt_addr,(u64)(uintptr_t)ctx->persist);
	KUNIT_EXPECT_EQ(test,md.entry.phys_addr,0x60000000ULL);
	KUNIT_EXPECT_EQ(test,mmi_annotate("RAM"),0);KUNIT_EXPECT_STREQ(test,(char *)mem_data.contents,"RAM");
	KUNIT_EXPECT_EQ(test,mmi_annotate_persist("persist"),0);KUNIT_EXPECT_STREQ(test,(char *)persist_data->contents,"persist");
	mmi_annotate_remove(f->pdev);expect_idle(test);KUNIT_EXPECT_EQ(test,md.removes,1U);
	KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);mmi_annotate_remove(f->pdev);expect_idle(test);
}
static void previous_boot_log_is_clamped_to_local_capacity(struct kunit *test)
{
	struct fixture *f=test->priv;struct persist_data_t *ram=mmi_annotate_map_ram(0x60000000,0x10000);
	KUNIT_ASSERT_NOT_NULL(test,ram);ram->magic_num=PERSIST_MAGIC_NUM;ram->size=SIZE_MAX;ram->cur_off=256;memset(ram->contents,'x',256);memunmap(ram);
	KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);
	KUNIT_EXPECT_EQ(test,mem_data.cur_off,63UL);KUNIT_EXPECT_EQ(test,mem_data.contents[63],(u8)0);
	KUNIT_EXPECT_EQ(test,strnlen((char *)mem_data.contents,64),63UL);KUNIT_EXPECT_EQ(test,persist_data->cur_off,0UL);
}
static void invalid_persistent_length_is_not_imported(struct kunit *test)
{
	struct fixture *f=test->priv;struct persist_data_t *ram=mmi_annotate_map_ram(0x60000000,0x10000);
	KUNIT_ASSERT_NOT_NULL(test,ram);ram->magic_num=PERSIST_MAGIC_NUM;ram->cur_off=SIZE_MAX;memunmap(ram);
	KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);KUNIT_EXPECT_EQ(test,mem_data.cur_off,0UL);
}
static void invalid_dt_inputs_fail_without_publishing(struct kunit *test)
{
	const char *names[]={"/avr-annotate-zero","/avr-annotate-missing","/avr-annotate-small"};int i;
	for(i=0;i<ARRAY_SIZE(names);i++) {
		struct platform_device *pdev=device_for(names[i]);KUNIT_ASSERT_NOT_NULL(test,pdev);
		KUNIT_EXPECT_EQ(test,mmi_annotate_probe(pdev),-EINVAL);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(pdev),NULL);put_device(&pdev->dev);expect_idle(test);
	}
	KUNIT_EXPECT_EQ(test,md.adds,0U);
}
static void minidump_add_failure_rolls_back_without_erasing_log(struct kunit *test)
{
	struct fixture *f=test->priv;struct persist_data_t *ram;
	ram=mmi_annotate_map_ram(0x60000000,0x10000);KUNIT_ASSERT_NOT_NULL(test,ram);ram->magic_num=PERSIST_MAGIC_NUM;ram->cur_off=3;memcpy(ram->contents,"old",4);memunmap(ram);
	md.fail_add=true;KUNIT_EXPECT_EQ(test,mmi_annotate_probe(f->pdev),-EIO);expect_idle(test);
	ram=mmi_annotate_map_ram(0x60000000,0x10000);KUNIT_ASSERT_NOT_NULL(test,ram);KUNIT_EXPECT_EQ(test,ram->cur_off,3UL);KUNIT_EXPECT_STREQ(test,(char *)ram->contents,"old");memunmap(ram);
	md.fail_add=false;KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);KUNIT_EXPECT_STREQ(test,(char *)mem_data.contents,"old");
}
static void failed_minidump_remove_retains_backing_and_retries(struct kunit *test)
{
	struct fixture *f=test->priv;struct annotate_provider *ctx;
	KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);ctx=active_provider;md.fail_remove=true;
	mmi_annotate_remove(f->pdev);KUNIT_EXPECT_PTR_EQ(test,retired_provider,ctx);KUNIT_EXPECT_TRUE(test,md.live);
	KUNIT_EXPECT_PTR_EQ(test,mem_data.contents,NULL);KUNIT_EXPECT_PTR_EQ(test,persist_data,NULL);
	KUNIT_EXPECT_EQ(test,((struct persist_data_t *)(uintptr_t)md.entry.virt_addr)->magic_num,(unsigned int)PERSIST_MAGIC_NUM);
	KUNIT_EXPECT_EQ(test,mmi_annotate_probe(f->pdev),-EIO);KUNIT_EXPECT_EQ(test,md.adds,1U);
	md.fail_remove=false;KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);KUNIT_EXPECT_PTR_EQ(test,retired_provider,NULL);KUNIT_EXPECT_EQ(test,md.adds,2U);
}
static void persistent_header_cannot_expand_mapping_capacity(struct kunit *test)
{
	struct fixture *f=test->priv;
	KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);persist_data->size=SIZE_MAX;persist_data->cur_off=persist_capacity;
	KUNIT_EXPECT_EQ(test,mmi_annotate_persist("x"),0);KUNIT_EXPECT_EQ(test,persist_data->cur_off,persist_capacity);
}
static void competing_provider_is_rejected(struct kunit *test)
{
	struct fixture *f=test->priv;struct platform_device *other=device_for("/avr-annotate-fallback");KUNIT_ASSERT_NOT_NULL(test,other);
	if(mmi_annotate_probe(f->pdev)){put_device(&other->dev);KUNIT_FAIL(test,"first probe failed");return;}
	KUNIT_EXPECT_EQ(test,mmi_annotate_probe(other),-EBUSY);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(other),NULL);put_device(&other->dev);
}
static void fallback_registers_and_writes_actual_ram_buffer(struct kunit *test)
{
	struct platform_device *pdev=device_for("/avr-annotate-fallback");int ret;KUNIT_ASSERT_NOT_NULL(test,pdev);
	ret=mmi_annotate_probe(pdev);KUNIT_EXPECT_EQ(test,ret,0);
	if(!ret){KUNIT_EXPECT_EQ(test,md.entry.virt_addr,(u64)(uintptr_t)mem_data.contents);KUNIT_EXPECT_EQ(test,md.entry.size,64ULL);KUNIT_EXPECT_EQ(test,mmi_annotate_persist("fallback"),0);KUNIT_EXPECT_STREQ(test,(char *)mem_data.contents,"fallback");KUNIT_EXPECT_EQ(test,persist_data->cur_off,0UL);mmi_annotate_remove(pdev);expect_idle(test);}
	put_device(&pdev->dev);
}
struct remove_work {struct fixture *f;struct completion started,done;};
static int remove_while_api_locked(void *arg)
{
	struct remove_work *work=arg;complete(&work->started);mmi_annotate_remove(work->f->pdev);complete(&work->done);
	while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static void removal_waits_for_api_lock_before_dump_release(struct kunit *test)
{
	struct fixture *f=test->priv;struct remove_work work={.f=f};struct task_struct *thread;int i;
	KUNIT_ASSERT_EQ(test,mmi_annotate_probe(f->pdev),0);init_completion(&work.started);init_completion(&work.done);
	mutex_lock(&mem_lock);
	thread=kthread_run(remove_while_api_locked,&work,"avr_annotate_remove");
	if(IS_ERR(thread)){mutex_unlock(&mem_lock);KUNIT_FAIL(test,"remove thread failed");return;}
	KUNIT_EXPECT_NE(test,wait_for_completion_timeout(&work.started,2*HZ),0UL);
	for(i=0;i<100 && !mutex_is_locked(&provider_lock);i++)usleep_range(1000,2000);
	KUNIT_EXPECT_TRUE(test,mutex_is_locked(&provider_lock));
	KUNIT_EXPECT_EQ(test,wait_for_completion_timeout(&work.done,msecs_to_jiffies(50)),0UL);
	KUNIT_EXPECT_TRUE(test,md.live);KUNIT_EXPECT_EQ(test,md.removes,0U);
	mutex_unlock(&mem_lock);kthread_stop(thread);KUNIT_EXPECT_TRUE(test,completion_done(&work.done));expect_idle(test);
}
struct writer_work {struct completion started;atomic_t attempts;};
static int write_during_lifecycle(void *arg)
{
	struct writer_work *work=arg;complete(&work->started);
	while(!kthread_should_stop()){mmi_annotate("x");mmi_annotate_persist("y");atomic_inc(&work->attempts);cond_resched();}
	return 0;
}
static void append_races_twenty_actual_probe_remove_cycles(struct kunit *test)
{
	struct fixture *f=test->priv;struct writer_work work;struct task_struct *thread;int i,ret;
	init_completion(&work.started);atomic_set(&work.attempts,0);thread=kthread_run(write_during_lifecycle,&work,"avr_annotate_writer");KUNIT_ASSERT_FALSE(test,IS_ERR(thread));
	KUNIT_EXPECT_NE(test,wait_for_completion_timeout(&work.started,2*HZ),0UL);
	for(i=0;i<20;i++) {
		ret=mmi_annotate_probe(f->pdev);KUNIT_EXPECT_EQ(test,ret,0);if(ret)break;
		usleep_range(1000,2000);mmi_annotate_remove(f->pdev);usleep_range(1000,2000);
	}
	kthread_stop(thread);KUNIT_EXPECT_GT(test,atomic_read(&work.attempts),0);expect_idle(test);
}
static struct kunit_case cases[]={KUNIT_CASE(successful_probe_append_and_remove),KUNIT_CASE(previous_boot_log_is_clamped_to_local_capacity),KUNIT_CASE(invalid_persistent_length_is_not_imported),KUNIT_CASE(invalid_dt_inputs_fail_without_publishing),KUNIT_CASE(minidump_add_failure_rolls_back_without_erasing_log),KUNIT_CASE(failed_minidump_remove_retains_backing_and_retries),KUNIT_CASE(persistent_header_cannot_expand_mapping_capacity),KUNIT_CASE(competing_provider_is_rejected),KUNIT_CASE(fallback_registers_and_writes_actual_ram_buffer),KUNIT_CASE(removal_waits_for_api_lock_before_dump_release),KUNIT_CASE(append_races_twenty_actual_probe_remove_cycles),{}};
static struct kunit_suite suite={.name="avr-mmi-annotate-driver",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
