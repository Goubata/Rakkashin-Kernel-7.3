// SPDX-License-Identifier: GPL-2.0-only
/* Exact production text append methods with private RAM globals. No probe,
 * persistent physical mapping or minidump API is executed by this test TU. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/module.h>
#pragma push_macro("module_init")
#pragma push_macro("module_exit")
#undef module_init
#undef module_exit
#define module_init(fn) static typeof(fn) *const __maybe_unused init_reference = fn
#define module_exit(fn) static typeof(fn) *const __maybe_unused exit_reference = fn
#include "../../ports/moto-info/mmi_annotate/mmi_annotate.c"
#pragma pop_macro("module_exit")
#pragma pop_macro("module_init")
struct fixture {u8 *ram,*persist;};
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
	if(!f)return -ENOMEM;
	f->ram=kunit_kmalloc(test,1056,GFP_KERNEL);f->persist=kunit_kmalloc(test,sizeof(*persist_data)+1056,GFP_KERNEL);
	if(!f->ram || !f->persist)return -ENOMEM;
	memset(f->ram,0xa5,1056);memset(f->ram+16,0,1024);
	memset(f->persist,0xa5,sizeof(*persist_data)+1056);
	mem_data=(struct mem_data_t){.size=1024,.contents=f->ram+16};
	persist_data=(void *)(f->persist+16);persist_data->size=1024;persist_data->cur_off=0;memset(persist_data->contents,0,1024);
	persist_capacity=1024;persist_unsupported=0;test->priv=f;return 0;
}
static void cleanup(struct kunit *test){mem_data=(struct mem_data_t){};persist_data=NULL;persist_capacity=0;persist_unsupported=0;}
static void guards(struct kunit *test)
{
	struct fixture *f=test->priv;int i;
	for(i=0;i<16;i++){KUNIT_EXPECT_EQ(test,f->ram[i],(u8)0xa5);KUNIT_EXPECT_EQ(test,f->ram[1040+i],(u8)0xa5);KUNIT_EXPECT_EQ(test,f->persist[i],(u8)0xa5);KUNIT_EXPECT_EQ(test,f->persist[sizeof(*persist_data)+1040+i],(u8)0xa5);}
}
static void short_text_append_is_terminated(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test,mmi_annotate("hello %u",7),0);KUNIT_EXPECT_EQ(test,mmi_annotate("%s","%literal"),0);
	KUNIT_EXPECT_STREQ(test,(char *)mem_data.contents,"hello 7%literal");KUNIT_EXPECT_EQ(test,mem_data.cur_off,15UL);guards(test);
}
static void long_format_copies_only_actual_stack_output(struct kunit *test)
{
	char *text=kunit_kmalloc(test,1501,GFP_KERNEL);KUNIT_ASSERT_NOT_NULL(test,text);memset(text,'x',1500);text[1500]=0;
	KUNIT_ASSERT_EQ(test,mmi_annotate("%s",text),0);KUNIT_EXPECT_EQ(test,mem_data.cur_off,511UL);
	KUNIT_EXPECT_EQ(test,mem_data.contents[511],(u8)0);KUNIT_EXPECT_EQ(test,mem_data.contents[512],(u8)0);guards(test);
}
static void full_buffer_reserves_terminator(struct kunit *test)
{
	mem_data.size=8;KUNIT_ASSERT_EQ(test,mmi_annotate("1234567"),0);KUNIT_ASSERT_EQ(test,mmi_annotate("8"),0);
	KUNIT_EXPECT_EQ(test,mem_data.cur_off,7UL);KUNIT_EXPECT_STREQ(test,(char *)mem_data.contents,"1234567");guards(test);
}
static void corrupt_offsets_are_rejected_without_wrap(struct kunit *test)
{
	mem_data.cur_off=SIZE_MAX-4;persist_data->cur_off=SIZE_MAX-4;
	KUNIT_EXPECT_EQ(test,mmi_annotate("1234567890"),0);KUNIT_EXPECT_EQ(test,mmi_annotate_persist("1234567890"),0);
	KUNIT_EXPECT_EQ(test,mem_data.contents[0],(u8)0);KUNIT_EXPECT_EQ(test,persist_data->contents[0],(u8)0);guards(test);
}
static void persistent_append_bounds_and_termination(struct kunit *test)
{
	char *text=kunit_kmalloc(test,1501,GFP_KERNEL);KUNIT_ASSERT_NOT_NULL(test,text);memset(text,'p',1500);text[1500]=0;
	KUNIT_EXPECT_EQ(test,mmi_annotate_persist("%s",text),0);KUNIT_EXPECT_EQ(test,persist_data->cur_off,511UL);
	KUNIT_EXPECT_EQ(test,persist_data->contents[511],(u8)0);persist_data->size=512;
	KUNIT_EXPECT_EQ(test,mmi_annotate_persist("x"),0);KUNIT_EXPECT_EQ(test,persist_data->cur_off,511UL);guards(test);
}
static void unsupported_persistence_uses_ram(struct kunit *test)
{
	persist_unsupported=1;KUNIT_EXPECT_EQ(test,mmi_annotate_persist("fallback"),0);
	KUNIT_EXPECT_STREQ(test,(char *)mem_data.contents,"fallback");KUNIT_EXPECT_EQ(test,persist_data->cur_off,0UL);guards(test);
}
static void null_formats_are_rejected(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test,mmi_annotate(NULL),-EINVAL);KUNIT_EXPECT_EQ(test,mmi_annotate_persist(NULL),-EINVAL);guards(test);
}
struct writer {struct completion done;};
static int write_text(void *arg)
{
	struct writer *w=arg;int i;for(i=0;i<100;i++)mmi_annotate("x");complete(&w->done);
	while(!kthread_should_stop())schedule_timeout_interruptible(HZ);return 0;
}
static void concurrent_writers_keep_exact_length(struct kunit *test)
{
	struct writer writers[4];struct task_struct *tasks[4]={};int i,j;
	for(i=0;i<4;i++) {
		init_completion(&writers[i].done);tasks[i]=kthread_run(write_text,&writers[i],"avr_annotate");
		if(IS_ERR(tasks[i])){tasks[i]=NULL;KUNIT_FAIL(test,"writer did not start");break;}
	}
	for(j=0;j<i;j++){KUNIT_EXPECT_NE(test,wait_for_completion_timeout(&writers[j].done,2*HZ),0UL);kthread_stop(tasks[j]);}
	KUNIT_EXPECT_EQ(test,mem_data.cur_off,(size_t)i*100);for(j=0;j<i*100;j++)KUNIT_EXPECT_EQ(test,mem_data.contents[j],(u8)'x');guards(test);
}
static struct kunit_case cases[]={KUNIT_CASE(short_text_append_is_terminated),KUNIT_CASE(long_format_copies_only_actual_stack_output),KUNIT_CASE(full_buffer_reserves_terminator),KUNIT_CASE(corrupt_offsets_are_rejected_without_wrap),KUNIT_CASE(persistent_append_bounds_and_termination),KUNIT_CASE(unsupported_persistence_uses_ram),KUNIT_CASE(null_formats_are_rejected),KUNIT_CASE(concurrent_writers_keep_exact_length),{}};
static struct kunit_suite suite={.name="avr-mmi-annotate-ram",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
