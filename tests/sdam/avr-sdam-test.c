// SPDX-License-Identifier: GPL-2.0-only
/* Actual factory callbacks and native NVMEM/regmap cores over RAM-only bus. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/nvmem-consumer.h>
#undef module_platform_driver
#define module_platform_driver(...)
#include "../../ports/qcom-sdam/qcom-spmi-sdam.c"
struct fixture {
	struct device *dev;
	struct sdam_chip chip;
	struct nvmem_device *nvmem;
	u8 ram[0x300];unsigned int calls,fail_at;
};
static int ram_read(void *ctx,unsigned int reg,unsigned int *value)
{
	struct fixture *f=ctx;if(++f->calls==f->fail_at)return -EIO;
	*value=f->ram[reg];return 0;
}
static int ram_write(void *ctx,unsigned int reg,unsigned int value)
{
	struct fixture *f=ctx;if(++f->calls==f->fail_at)return -EIO;
	f->ram[reg]=value;return 0;
}
static const struct regmap_bus bus={.reg_read=ram_read,.reg_write=ram_write};
static const struct regmap_config config={.reg_bits=16,.val_bits=8,.max_register=0x2ff,.cache_type=REGCACHE_NONE};
static int setup(struct kunit *test)
{
	struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
	if(!f)return -ENOMEM;
	f->dev=root_device_register("avr_sdam_ram_test");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
	f->chip.regmap=regmap_init(f->dev,&bus,f,&config);
	if(IS_ERR(f->chip.regmap)){ret=PTR_ERR(f->chip.regmap);goto unregister;}
	f->chip.base=0x100;f->chip.size=128;
	f->chip.sdam_config=(struct nvmem_config){.dev=f->dev,.name="avr_sdam_ram",.id=NVMEM_DEVID_AUTO,.owner=THIS_MODULE,.stride=1,.word_size=1,.reg_read=sdam_read,.reg_write=sdam_write,.priv=&f->chip};
	/* Factory intentionally leaves config.size unset. Preserve its ABI. */
	f->nvmem=nvmem_register(&f->chip.sdam_config);
	if(IS_ERR(f->nvmem)){ret=PTR_ERR(f->nvmem);regmap_exit(f->chip.regmap);goto unregister;}
	test->priv=f;return 0;
unregister:root_device_unregister(f->dev);return ret;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f=test->priv;nvmem_unregister(f->nvmem);regmap_exit(f->chip.regmap);root_device_unregister(f->dev);
}
static void native_nvmem_roundtrip_and_guards(struct kunit *test)
{
	struct fixture *f=test->priv;u8 input[123],output[123];int i;
	memset(f->ram,0x5a,sizeof(f->ram));for(i=0;i<123;i++)input[i]=i^0xa5;
	KUNIT_ASSERT_EQ(test,nvmem_device_write(f->nvmem,0x45,sizeof(input),input),123);
	KUNIT_ASSERT_EQ(test,nvmem_device_read(f->nvmem,0x45,sizeof(output),output),123);
	KUNIT_EXPECT_MEMEQ(test,input,output,sizeof(input));
	for(i=0;i<sizeof(f->ram);i++)if(i<0x145 || i>0x1bf)KUNIT_EXPECT_EQ(test,f->ram[i],(u8)0x5a);
}
static void range_and_zero_length_rejected(struct kunit *test)
{
	struct fixture *f=test->priv;u8 value=0;unsigned int offsets[]={0,0x3f,0xc0,UINT_MAX};int i;
	for(i=0;i<ARRAY_SIZE(offsets);i++)KUNIT_EXPECT_EQ(test,sdam_read(&f->chip,offsets[i],&value,1),-EINVAL);
	KUNIT_EXPECT_EQ(test,sdam_read(&f->chip,0x45,&value,0),-EINVAL);
	KUNIT_EXPECT_EQ(test,sdam_write(&f->chip,0xbf,&value,2),-EINVAL);
	KUNIT_EXPECT_EQ(test,f->calls,0U);
}
static void overflowing_length_is_rejected(struct kunit *test)
{
	struct fixture *f=test->priv;
	/* Predicate only: never pass intentionally huge size to any bus/copy API. */
	KUNIT_EXPECT_FALSE(test,sdam_is_valid(&f->chip,0x45,SIZE_MAX));
	KUNIT_EXPECT_FALSE(test,sdam_is_valid(&f->chip,0x45,SIZE_MAX-0x40));
	KUNIT_EXPECT_FALSE(test,sdam_is_valid(&f->chip,0xbf,2));
	KUNIT_EXPECT_TRUE(test,sdam_is_valid(&f->chip,0xbf,1));
}
static void readonly_registers_and_spans_protected(struct kunit *test)
{
	struct fixture *f=test->priv;u8 value[8]={1};unsigned int ro[]={0x40,0x41,0x44};int i;
	for(i=0;i<ARRAY_SIZE(ro);i++)KUNIT_EXPECT_EQ(test,nvmem_device_write(f->nvmem,ro[i],1,value),-EINVAL);
	KUNIT_EXPECT_EQ(test,nvmem_device_write(f->nvmem,0x42,4,value),-EINVAL);
	KUNIT_EXPECT_EQ(test,f->calls,0U);
}
static void pbs_triggers_are_single_byte_exceptions(struct kunit *test)
{
	struct fixture *f=test->priv;u8 value=0xa7,out=0;unsigned int offsets[]={0xe5,0xe6};int i;
	for(i=0;i<ARRAY_SIZE(offsets);i++) {
		KUNIT_ASSERT_EQ(test,nvmem_device_write(f->nvmem,offsets[i],1,&value),1);
		KUNIT_ASSERT_EQ(test,nvmem_device_read(f->nvmem,offsets[i],1,&out),1);
		KUNIT_EXPECT_EQ(test,out,value);
		KUNIT_EXPECT_FALSE(test,sdam_is_valid(&f->chip,offsets[i],2));
	}
}
static void bus_read_write_errors_propagate(struct kunit *test)
{
	struct fixture *f=test->priv;u8 value[4]={1,2,3,4};unsigned int i;
	for(i=1;i<=4;i++) {
		f->calls=0;f->fail_at=i;KUNIT_EXPECT_EQ(test,nvmem_device_read(f->nvmem,0x50,4,value),-EIO);KUNIT_EXPECT_EQ(test,f->calls,i);
		f->calls=0;KUNIT_EXPECT_EQ(test,nvmem_device_write(f->nvmem,0x50,4,value),-EIO);KUNIT_EXPECT_EQ(test,f->calls,i);
	}
}
static void actual_readonly_metadata_is_readable(struct kunit *test)
{
	struct fixture *f=test->priv;u8 output[5]={};
	memcpy(f->ram+0x140,"ABCDE",5);
	KUNIT_ASSERT_EQ(test,nvmem_device_read(f->nvmem,0x40,5,output),5);
	KUNIT_EXPECT_MEMEQ(test,output,"ABCDE",5);
}
static void zero_capacity_has_no_data_window(struct kunit *test)
{
	struct fixture *f=test->priv;f->chip.size=0;
	KUNIT_EXPECT_FALSE(test,sdam_is_valid(&f->chip,0x40,1));
	KUNIT_EXPECT_TRUE(test,sdam_is_valid(&f->chip,0xe5,1));
}
static struct kunit_case cases[]={KUNIT_CASE(native_nvmem_roundtrip_and_guards),KUNIT_CASE(range_and_zero_length_rejected),KUNIT_CASE(overflowing_length_is_rejected),KUNIT_CASE(readonly_registers_and_spans_protected),KUNIT_CASE(pbs_triggers_are_single_byte_exceptions),KUNIT_CASE(bus_read_write_errors_propagate),KUNIT_CASE(actual_readonly_metadata_is_readable),KUNIT_CASE(zero_capacity_has_no_data_window),{}};
static struct kunit_suite suite={.name="avr-sdam-factory-ram",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
