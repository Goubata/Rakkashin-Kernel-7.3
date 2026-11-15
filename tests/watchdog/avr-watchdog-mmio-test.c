// SPDX-License-Identifier: GPL-2.0-only
/* Exact production operations; raw IO writes target only private guest RAM. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/math64.h>
#include <linux/overflow.h>
#include <soc/qcom/watchdog.h>
#include "generated-mmio.h"
struct fixture {struct qcom_soc_wdt *a,*b;u32 *ram_a,*ram_b;};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);if(!f)return -ENOMEM;test->priv=f;
 f->a=kunit_kzalloc(test,sizeof(*f->a),GFP_KERNEL);f->b=kunit_kzalloc(test,sizeof(*f->b),GFP_KERNEL);
 f->ram_a=kunit_kzalloc(test,4096,GFP_KERNEL);f->ram_b=kunit_kzalloc(test,4096,GFP_KERNEL);
 if(!f->a||!f->b||!f->ram_a||!f->ram_b)return -ENOMEM;
 f->a->map=f->b->map=wdt_default_map;f->a->core.base=(void __iomem *)f->ram_a;f->b->core.base=(void __iomem *)f->ram_b;return 0;
}
static void saved_intervals_program_exact_ticks(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,qcom_soc_set_wdt_bark(11000,&f->a->core),0);KUNIT_ASSERT_EQ(test,qcom_soc_set_wdt_bite(14000,&f->a->core),0);
 KUNIT_EXPECT_EQ(test,f->ram_a[0x10/4],360415u);KUNIT_EXPECT_EQ(test,f->ram_a[0x14/4],458710u);
}
static void reset_enable_disable_preserve_other_registers(struct kunit *test)
{
 struct fixture *f=test->priv;f->ram_a[0xc/4]=0xabcdef01;f->ram_a[0x10/4]=123;
 KUNIT_EXPECT_EQ(test,qcom_soc_reset_wdt(&f->a->core),0);KUNIT_EXPECT_EQ(test,f->ram_a[1],1u);
 KUNIT_EXPECT_EQ(test,qcom_soc_enable_wdt(3,&f->a->core),0);KUNIT_EXPECT_EQ(test,f->ram_a[2],3u);
 KUNIT_EXPECT_EQ(test,qcom_soc_disable_wdt(&f->a->core),0);KUNIT_EXPECT_EQ(test,f->ram_a[2],0u);
 KUNIT_EXPECT_EQ(test,f->ram_a[3],0xabcdef01u);KUNIT_EXPECT_EQ(test,f->ram_a[4],123u);
}
static void invalid_time_and_rate_leave_output_and_mmio_unchanged(struct kunit *test)
{
 struct fixture *f=test->priv;u32 ticks=0xa5;f->ram_a[4]=0xa5;
 KUNIT_EXPECT_EQ(test,qcom_soc_wdt_ticks(0,32765,&ticks),-EINVAL);KUNIT_EXPECT_EQ(test,ticks,0xa5u);
 KUNIT_EXPECT_EQ(test,qcom_soc_wdt_ticks(1000,0,&ticks),-EINVAL);KUNIT_EXPECT_EQ(test,ticks,0xa5u);
 KUNIT_EXPECT_EQ(test,qcom_soc_set_wdt_bark(0,&f->a->core),-EINVAL);KUNIT_EXPECT_EQ(test,f->ram_a[4],0xa5u);
}
static void multiplication_and_register_overflow_are_rejected(struct kunit *test)
{
 struct fixture *f=test->priv;u32 ticks=0xa5;f->ram_a[5]=0xa5;
 KUNIT_EXPECT_EQ(test,qcom_soc_wdt_ticks(U32_MAX,ULONG_MAX,&ticks),-ERANGE);KUNIT_EXPECT_EQ(test,ticks,0xa5u);
 KUNIT_EXPECT_EQ(test,qcom_soc_set_wdt_bite(U32_MAX,&f->a->core),-ERANGE);KUNIT_EXPECT_EQ(test,f->ram_a[5],0xa5u);
 KUNIT_EXPECT_EQ(test,qcom_soc_wdt_ticks(1000,U32_MAX,&ticks),0);KUNIT_EXPECT_EQ(test,ticks,U32_MAX);
}
static void sub_tick_intervals_are_not_silently_zeroed(struct kunit *test)
{
 u32 ticks=0xa5;KUNIT_EXPECT_EQ(test,qcom_soc_wdt_ticks(1,1,&ticks),-ERANGE);KUNIT_EXPECT_EQ(test,ticks,0xa5u);
 KUNIT_EXPECT_EQ(test,qcom_soc_wdt_ticks(1,32765,&ticks),0);KUNIT_EXPECT_EQ(test,ticks,32u);
}
static void per_device_register_maps_do_not_leak_between_instances(struct kunit *test)
{
 struct fixture *f=test->priv;f->a->map.wdt0_bark_time=0x80;f->a->map.wdt_hz=1000;
 KUNIT_EXPECT_EQ(test,qcom_soc_set_wdt_bark(11000,&f->a->core),0);KUNIT_EXPECT_EQ(test,qcom_soc_set_wdt_bark(11000,&f->b->core),0);
 KUNIT_EXPECT_EQ(test,f->ram_a[0x80/4],11000u);KUNIT_EXPECT_EQ(test,f->ram_a[0x10/4],0u);
 KUNIT_EXPECT_EQ(test,f->ram_b[0x10/4],360415u);KUNIT_EXPECT_EQ(test,f->ram_b[0x80/4],0u);
}
static struct kunit_case cases[]={KUNIT_CASE(saved_intervals_program_exact_ticks),KUNIT_CASE(reset_enable_disable_preserve_other_registers),KUNIT_CASE(invalid_time_and_rate_leave_output_and_mmio_unchanged),KUNIT_CASE(multiplication_and_register_overflow_are_rejected),KUNIT_CASE(sub_tick_intervals_are_not_silently_zeroed),KUNIT_CASE(per_device_register_maps_do_not_leak_between_instances),{}};
static struct kunit_suite suite={.name="avr-watchdog-mmio",.init=setup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");

MODULE_DESCRIPTION("Watchdog register operations on private guest RAM");
