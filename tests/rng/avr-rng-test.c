// SPDX-License-Identifier: GPL-2.0
/* Exact production read callbacks with native polling/clock APIs and recorded
 * MMIO words. Never register a synthetic entropy provider. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/clk-provider.h>
static u32 recorded_read(const volatile void __iomem *addr);
#undef readl
#undef readl_relaxed
#define readl recorded_read
#define readl_relaxed recorded_read
#undef module_platform_driver
#define module_platform_driver(driver)
#include "../../ports/qcom-rng/qcom-rng.c"
#undef readl
#undef readl_relaxed
struct fixture {
 struct qcom_rng rng;u32 registers[2],words[32];unsigned int count,next,status_reads,data_reads,ready_after;
 struct kunit *test;
};
static struct fixture *backend;
static u32 recorded_read(const volatile void __iomem *addr)
{
 struct fixture *f=backend;unsigned long offset=(unsigned long)addr-(unsigned long)f->rng.base;
 if(offset==PRNG_STATUS){f->status_reads++;return f->status_reads>=f->ready_after && f->next<f->count ? PRNG_STATUS_DATA_AVAIL : 0;}
 if(offset==PRNG_DATA_OUT){f->data_reads++;if(f->next<f->count)return f->words[f->next++];KUNIT_FAIL(f->test,"read past recorded FIFO");return 0;}
 KUNIT_FAIL(f->test,"unexpected register offset %lu",offset);return 0;
}
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);unsigned int i;
 if(!f)return -ENOMEM;test->priv=f;f->test=test;f->rng.base=(void __iomem *)f->registers;
 for(i=0;i<ARRAY_SIZE(f->words);i++)f->words[i]=0x12345600+i;backend=f;return 0;
}
static void cleanup(struct kunit *test){(void)test;backend=NULL;}
static void zero_request_never_accesses_mmio(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,NULL,0,true),0);
 KUNIT_EXPECT_EQ(test,f->status_reads,0u);KUNIT_EXPECT_EQ(test,f->data_reads,0u);
}
static void nonblocking_empty_fifo_is_one_status_read(struct kunit *test)
{
 struct fixture *f=test->priv;u8 data[4]={0xa5,0xa5,0xa5,0xa5};
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,data,sizeof(data),false),0);
 KUNIT_EXPECT_EQ(test,f->status_reads,1u);KUNIT_EXPECT_EQ(test,f->data_reads,0u);KUNIT_EXPECT_EQ(test,data[0],0xa5);
}
static void every_tail_and_unaligned_output_preserves_guards(struct kunit *test)
{
 struct fixture *f=test->priv;u8 data[80];unsigned int n,i;
 for(n=1;n<=65;n++){
  f->count=ARRAY_SIZE(f->words);f->next=f->status_reads=f->data_reads=0;memset(data,0xa5,sizeof(data));
  KUNIT_ASSERT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,data+1,n,false),(int)n);
  KUNIT_EXPECT_MEMEQ(test,data+1,f->words,n);KUNIT_EXPECT_EQ(test,data[0],0xa5);
  for(i=n+1;i<sizeof(data);i++)KUNIT_EXPECT_EQ(test,data[i],0xa5);
  KUNIT_EXPECT_EQ(test,f->data_reads,DIV_ROUND_UP(n,4));
 }
}
static void nonblocking_partial_result_keeps_copied_bytes(struct kunit *test)
{
 struct fixture *f=test->priv;u8 data[12];memset(data,0xa5,sizeof(data));f->count=1;
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,data,sizeof(data),false),4);
 KUNIT_EXPECT_MEMEQ(test,data,f->words,4);KUNIT_EXPECT_EQ(test,data[4],0xa5);KUNIT_EXPECT_EQ(test,f->status_reads,2u);
}
static void blocking_empty_fifo_times_out_without_data(struct kunit *test)
{
 struct fixture *f=test->priv;u8 data[4]={0xa5};
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,data,sizeof(data),true),-ETIMEDOUT);
 KUNIT_EXPECT_GT(test,f->status_reads,1u);KUNIT_EXPECT_EQ(test,f->data_reads,0u);KUNIT_EXPECT_EQ(test,data[0],0xa5);
}
static void blocking_timeout_returns_an_already_copied_prefix(struct kunit *test)
{
 struct fixture *f=test->priv;u8 data[8];memset(data,0xa5,sizeof(data));f->count=1;
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,data,sizeof(data),true),4);
 KUNIT_EXPECT_MEMEQ(test,data,f->words,4);KUNIT_EXPECT_EQ(test,data[4],0xa5);KUNIT_EXPECT_EQ(test,f->data_reads,1u);
}
static void blocking_poll_observes_later_recorded_readiness(struct kunit *test)
{
 struct fixture *f=test->priv;u8 data[4];f->count=1;f->ready_after=3;
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,data,sizeof(data),true),4);
 KUNIT_EXPECT_MEMEQ(test,data,f->words,4);KUNIT_EXPECT_EQ(test,f->status_reads,3u);
}
static void zero_word_is_data_and_not_a_fifo_empty_marker(struct kunit *test)
{
 struct fixture *f=test->priv;u8 data[8];f->count=2;f->words[0]=0;
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,data,sizeof(data),false),8);KUNIT_EXPECT_MEMEQ(test,data,f->words,8);
}
static void size_t_request_does_not_wrap_to_a_zero_byte_operation(struct kunit *test)
{
 struct fixture *f=test->priv;u8 byte=0xa5;
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,&byte,(size_t)UINT_MAX+1,false),0);
 KUNIT_EXPECT_EQ(test,f->status_reads,1u);KUNIT_EXPECT_EQ(test,f->data_reads,0u);KUNIT_EXPECT_EQ(test,byte,0xa5);
 KUNIT_EXPECT_EQ(test,qcom_hwrng_read(&f->rng.hwrng,NULL,1,false),-EINVAL);
}
static void optional_absent_clock_has_a_balanced_native_lifecycle(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_EXPECT_EQ(test,qcom_hwrng_init(&f->rng.hwrng),0);qcom_hwrng_cleanup(&f->rng.hwrng);
 KUNIT_EXPECT_EQ(test,f->status_reads,0u);
}
static void real_fixed_clock_prepare_and_cleanup_balance(struct kunit *test)
{
 struct fixture *f=test->priv;struct clk_hw *hw=clk_hw_register_fixed_rate(NULL,"avr-rng-ram-clock",NULL,0,19200000);
 KUNIT_ASSERT_FALSE(test,IS_ERR(hw));f->rng.clk=clk_hw_get_clk(hw,NULL);
 if(IS_ERR(f->rng.clk)){KUNIT_FAIL(test,"native clock consumer failed");f->rng.clk=NULL;clk_hw_unregister_fixed_rate(hw);return;}
 KUNIT_EXPECT_FALSE(test,clk_hw_is_prepared(hw));KUNIT_EXPECT_EQ(test,qcom_hwrng_init(&f->rng.hwrng),0);
 KUNIT_EXPECT_TRUE(test,clk_hw_is_prepared(hw));qcom_hwrng_cleanup(&f->rng.hwrng);KUNIT_EXPECT_FALSE(test,clk_hw_is_prepared(hw));
 clk_put(f->rng.clk);f->rng.clk=NULL;clk_hw_unregister_fixed_rate(hw);
}
static struct kunit_case cases[]={KUNIT_CASE(zero_request_never_accesses_mmio),KUNIT_CASE(nonblocking_empty_fifo_is_one_status_read),KUNIT_CASE(every_tail_and_unaligned_output_preserves_guards),KUNIT_CASE(nonblocking_partial_result_keeps_copied_bytes),KUNIT_CASE(blocking_empty_fifo_times_out_without_data),KUNIT_CASE(blocking_timeout_returns_an_already_copied_prefix),KUNIT_CASE(blocking_poll_observes_later_recorded_readiness),KUNIT_CASE(zero_word_is_data_and_not_a_fifo_empty_marker),KUNIT_CASE(size_t_request_does_not_wrap_to_a_zero_byte_operation),KUNIT_CASE(optional_absent_clock_has_a_balanced_native_lifecycle),KUNIT_CASE(real_fixed_clock_prepare_and_cleanup_balance),{}};
static struct kunit_suite suite={.name="avr-rng",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
