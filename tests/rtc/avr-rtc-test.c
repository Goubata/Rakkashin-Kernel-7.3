// SPDX-License-Identifier: GPL-2.0-only
/* Actual native RTC callbacks over the real regmap core and a RAM-only bus.
 * This does not model physical SPMI, PMIC timekeeping or wake IRQ delivery. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(...)
#include "../../ports/qcom-rtc/rtc-pm8xxx.c"

struct fixture {
	struct device *dev;
	struct pm8xxx_rtc rtc;
	u8 *ram;
	unsigned int calls,fail_at,read_count;
	bool rollover;
};
static int ram_read(void *ctx,unsigned int reg,unsigned int *value)
{
	struct fixture *f=ctx;
	if(++f->calls==f->fail_at)return -EIO;
	if(reg==pmk8350_regs.read && ++f->read_count==2 && f->rollover)
		put_unaligned_le32(0x12345700,f->ram+pmk8350_regs.read);
	*value=f->ram[reg];return 0;
}
static int ram_write(void *ctx,unsigned int reg,unsigned int value)
{
	struct fixture *f=ctx;
	if(++f->calls==f->fail_at)return -EIO;
	f->ram[reg]=value;return 0;
}
static const struct regmap_bus bus={.reg_read=ram_read,.reg_write=ram_write};
static const struct regmap_config config={.reg_bits=16,.val_bits=8,.max_register=0x62ff,.cache_type=REGCACHE_NONE};
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
	if(!f)return -ENOMEM;
	f->ram=kunit_kzalloc(test,0x6300,GFP_KERNEL);if(!f->ram)return -ENOMEM;
	f->dev=root_device_register("avr_rtc_ram_test");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
	f->rtc.regmap=regmap_init(f->dev,&bus,f,&config);
	if(IS_ERR(f->rtc.regmap)){int ret=PTR_ERR(f->rtc.regmap);root_device_unregister(f->dev);return ret;}
	f->rtc.dev=f->dev;f->rtc.regs=&pmk8350_regs;dev_set_drvdata(f->dev,&f->rtc);
	f->rtc.rtc=devm_rtc_allocate_device(f->dev);
	if(IS_ERR(f->rtc.rtc)){int ret=PTR_ERR(f->rtc.rtc);regmap_exit(f->rtc.regmap);root_device_unregister(f->dev);return ret;}
	f->rtc.rtc->ops=&pm8xxx_rtc_ops;f->rtc.rtc->range_max=U32_MAX;
	test->priv=f;return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f=test->priv;
	cancel_work_sync(&f->rtc.rtc->irqwork);
	regmap_exit(f->rtc.regmap);root_device_unregister(f->dev);
}
static void time_decode_and_offset(struct kunit *test)
{
	struct fixture *f=test->priv;struct rtc_time tm;u32 secs=1700000000;
	put_unaligned_le32(secs,f->ram+pmk8350_regs.read);f->rtc.offset=1234;
	KUNIT_ASSERT_EQ(test,pm8xxx_rtc_read_time(f->dev,&tm),0);
	KUNIT_EXPECT_EQ(test,rtc_tm_to_time64(&tm),(time64_t)secs+1234);
}
static void carry_over_rereads_counter(struct kunit *test)
{
	struct fixture *f=test->priv;u32 secs=0;
	put_unaligned_le32(0x123456ff,f->ram+pmk8350_regs.read);f->rollover=true;
	KUNIT_ASSERT_EQ(test,pm8xxx_rtc_read_raw(&f->rtc,&secs),0);
	KUNIT_EXPECT_EQ(test,secs,0x12345700U);KUNIT_EXPECT_EQ(test,f->read_count,3U);
}
static void alarm_roundtrip_with_offset(struct kunit *test)
{
	struct fixture *f=test->priv;struct rtc_wkalrm in={.enabled=1},out={};u32 secs=1800000000;
	f->rtc.offset=4321;rtc_time64_to_tm(secs,&in.time);
	f->ram[pmk8350_regs.alarm_ctrl]=0x25;
	KUNIT_ASSERT_EQ(test,pm8xxx_rtc_set_alarm(f->dev,&in),0);
	KUNIT_EXPECT_EQ(test,get_unaligned_le32(f->ram+pmk8350_regs.alarm_rw),secs-4321);
	KUNIT_EXPECT_EQ(test,f->ram[pmk8350_regs.alarm_ctrl],(u8)0xa5);
	KUNIT_ASSERT_EQ(test,pm8xxx_rtc_read_alarm(f->dev,&out),0);
	KUNIT_EXPECT_EQ(test,rtc_tm_to_time64(&out.time),(time64_t)secs);KUNIT_EXPECT_TRUE(test,out.enabled);
	KUNIT_ASSERT_EQ(test,pm8xxx_rtc_alarm_irq_enable(f->dev,0),0);
	KUNIT_EXPECT_EQ(test,get_unaligned_le32(f->ram+pmk8350_regs.alarm_rw),0U);
	KUNIT_EXPECT_EQ(test,f->ram[pmk8350_regs.alarm_ctrl],(u8)0x25);
}
static void read_errors_do_not_publish_time(struct kunit *test)
{
	struct fixture *f=test->priv;unsigned int i;u32 secs;
	for(i=1;i<=5;i++) {
		secs=0xdeadbeef;f->calls=0;f->fail_at=i;
		KUNIT_EXPECT_EQ(test,pm8xxx_rtc_read_raw(&f->rtc,&secs),-EIO);
		KUNIT_EXPECT_EQ(test,secs,0xdeadbeefU);
	}
}
static void alarm_bus_errors_are_returned(struct kunit *test)
{
	struct fixture *f=test->priv;struct rtc_wkalrm alarm={.enabled=1};unsigned int i;
	rtc_time64_to_tm(1800000000,&alarm.time);
	/* Disable read/write, four data bytes, then enable read/write. */
	for(i=1;i<=8;i++) {
		f->calls=0;f->fail_at=i;f->ram[pmk8350_regs.alarm_ctrl]=0x80;
		KUNIT_EXPECT_EQ(test,pm8xxx_rtc_set_alarm(f->dev,&alarm),-EIO);
		KUNIT_EXPECT_EQ(test,f->calls,i);
	}
}
static void no_storage_time_write_is_rejected(struct kunit *test)
{
	struct fixture *f=test->priv;struct rtc_time tm;
	rtc_time64_to_tm(1800000000,&tm);
	KUNIT_EXPECT_EQ(test,pm8xxx_rtc_set_time(f->dev,&tm),-ENODEV);
	KUNIT_EXPECT_EQ(test,f->calls,0U);
}
static void direct_set_time_restores_alarm(struct kunit *test)
{
	struct fixture *f=test->priv;struct rtc_time tm;u32 secs=1800000000;
	f->rtc.allow_set_time=true;f->ram[pmk8350_regs.ctrl]=0x80;f->ram[pmk8350_regs.alarm_ctrl]=0xa5;
	rtc_time64_to_tm(secs,&tm);
	KUNIT_ASSERT_EQ(test,pm8xxx_rtc_set_time(f->dev,&tm),0);
	KUNIT_EXPECT_EQ(test,get_unaligned_le32(f->ram+pmk8350_regs.write),secs);
	KUNIT_EXPECT_EQ(test,f->ram[pmk8350_regs.ctrl],(u8)0x80);
	KUNIT_EXPECT_EQ(test,f->ram[pmk8350_regs.alarm_ctrl],(u8)0xa5);
}
static void alarm_irq_disables_and_acknowledges(struct kunit *test)
{
	struct fixture *f=test->priv;struct rtc_wkalrm alarm={.enabled=1};
	put_unaligned_le32(1700000000,f->ram+pmk8350_regs.read);
	rtc_time64_to_tm(1700000060,&alarm.time);
	KUNIT_ASSERT_EQ(test,rtc_set_alarm(f->rtc.rtc,&alarm),0);
	/* Advance RAM counter to the programmed expiry; use actual RTC work. */
	put_unaligned_le32(1700000060,f->ram+pmk8350_regs.read);
	f->ram[pmk8350_regs.alarm_ctrl2]=0x91;
	KUNIT_EXPECT_EQ(test,pm8xxx_alarm_trigger(1,&f->rtc),IRQ_HANDLED);
	flush_work(&f->rtc.rtc->irqwork);
	KUNIT_EXPECT_EQ(test,f->ram[pmk8350_regs.alarm_ctrl],(u8)0);
	KUNIT_EXPECT_EQ(test,f->ram[pmk8350_regs.alarm_ctrl2],(u8)0x90);
	KUNIT_EXPECT_EQ(test,f->rtc.rtc->irq_data,1UL<<8 | RTC_IRQF | RTC_AF);
}
static struct kunit_case cases[]={KUNIT_CASE(time_decode_and_offset),KUNIT_CASE(carry_over_rereads_counter),KUNIT_CASE(alarm_roundtrip_with_offset),KUNIT_CASE(read_errors_do_not_publish_time),KUNIT_CASE(alarm_bus_errors_are_returned),KUNIT_CASE(no_storage_time_write_is_rejected),KUNIT_CASE(direct_set_time_restores_alarm),KUNIT_CASE(alarm_irq_disables_and_acknowledges),{}};
static struct kunit_suite suite={.name="avr-rtc-native-ram",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
