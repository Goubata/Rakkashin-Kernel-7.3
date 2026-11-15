// SPDX-License-Identifier: GPL-2.0-only
/* Exact production callbacks and native regmap on private RAM. No PMIC IO. */
#include <kunit/test.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(drv) static struct platform_driver * const __maybe_unused tested_driver = &(drv)
#include "../../ports/usb-repeater/repeater-qti-pmic-eusb2.c"
struct fixture {struct eusb2_repeater er;struct device *dev;u8 *regs;unsigned int fail_reg;bool fail_read,fail_write;unsigned int status_reads;};
static int ram_read(void *context,unsigned int reg,unsigned int *value)
{
 struct fixture *f=context;if(reg==EUSB2_RPTR_STATUS)f->status_reads++;
 if(f->fail_read && reg==f->fail_reg)return -EIO;
 if(reg>=256)return -EINVAL;*value=f->regs[reg];return 0;
}
static int ram_write(void *context,unsigned int reg,unsigned int value)
{
 struct fixture *f=context;if(f->fail_write && reg==f->fail_reg)return -EIO;
 if(reg>=256)return -EINVAL;f->regs[reg]=value;return 0;
}
static const struct regmap_bus bus={.reg_read=ram_read,.reg_write=ram_write};
static const struct regmap_config config={.reg_bits=16,.val_bits=8,.max_register=255,.cache_type=REGCACHE_NONE};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);if(!f)return -ENOMEM;
 test->priv=f;f->regs=kunit_kzalloc(test,256,GFP_KERNEL);if(!f->regs)return -ENOMEM;
 f->dev=root_device_register("avr-eusb2-ram");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
 f->er.ur.dev=f->dev;dev_set_drvdata(f->dev,&f->er);mutex_init(&f->er.user_param_lock);
 f->er.regmap=regmap_init(f->dev,&bus,f,&config);if(IS_ERR(f->er.regmap)){int ret=PTR_ERR(f->er.regmap);root_device_unregister(f->dev);return ret;}
 return 0;
}
static void cleanup(struct kunit *test){struct fixture *f=test->priv;regmap_exit(f->er.regmap);root_device_unregister(f->dev);}
static void version_reads_real_regmap(struct kunit *test){struct fixture *f=test->priv;f->regs[EUSB2_REVISION1]=0x23;KUNIT_EXPECT_EQ(test,eusb2_repeater_get_version(&f->er.ur),0x23);}
static void failed_version_is_negative(struct kunit *test){struct fixture *f=test->priv;f->fail_read=true;f->fail_reg=EUSB2_REVISION1;KUNIT_EXPECT_EQ(test,eusb2_repeater_get_version(&f->er.ur),-EIO);}
static void reset_updates_and_clears_mask(struct kunit *test){struct fixture *f=test->priv;f->regs[EUSB2_EN_CTL1]=0x12;KUNIT_EXPECT_EQ(test,eusb2_repeater_reset(&f->er.ur,true),0);KUNIT_EXPECT_EQ(test,f->regs[EUSB2_EN_CTL1],(u8)(0x12|EUSB2_RPTR_EN));KUNIT_EXPECT_EQ(test,eusb2_repeater_reset(&f->er.ur,false),0);KUNIT_EXPECT_EQ(test,f->regs[EUSB2_EN_CTL1],(u8)0x12);}
static void failed_reset_is_negative(struct kunit *test){struct fixture *f=test->priv;f->fail_write=true;f->fail_reg=EUSB2_EN_CTL1;KUNIT_EXPECT_EQ(test,eusb2_repeater_reset(&f->er.ur,true),-EIO);}
static void init_requires_ready_status(struct kunit *test){struct fixture *f=test->priv;KUNIT_EXPECT_EQ(test,eusb2_repeater_init(&f->er.ur),-ETIMEDOUT);KUNIT_EXPECT_EQ(test,f->status_reads,(unsigned int)INIT_MAX_CNT);}
static void init_propagates_status_read_error(struct kunit *test){struct fixture *f=test->priv;f->fail_read=true;f->fail_reg=EUSB2_RPTR_STATUS;KUNIT_EXPECT_EQ(test,eusb2_repeater_init(&f->er.ur),-EIO);KUNIT_EXPECT_EQ(test,f->status_reads,1u);}
static void init_accepts_ready_status(struct kunit *test){struct fixture *f=test->priv;f->regs[EUSB2_RPTR_STATUS]=RPTR_OK;KUNIT_EXPECT_EQ(test,eusb2_repeater_init(&f->er.ur),0);KUNIT_EXPECT_EQ(test,f->status_reads,1u);}
static void user_parser_handles_nonterminated_count(struct kunit *test)
{
 struct fixture *f=test->priv;const char raw[]={'1',',','2','X'};
 KUNIT_EXPECT_EQ(test,user_param_override_store(f->dev,NULL,raw,3),(ssize_t)3);
 KUNIT_ASSERT_EQ(test,f->er.user_param_override_seq_cnt,(u8)2);
 KUNIT_EXPECT_EQ(test,f->er.user_param_override_seq[0],1u);KUNIT_EXPECT_EQ(test,f->er.user_param_override_seq[1],2u);
}
static void invalid_replacement_preserves_old_sequence(struct kunit *test)
{
 struct fixture *f=test->priv;u32 *old;
 KUNIT_ASSERT_EQ(test,user_param_override_store(f->dev,NULL,"1,2",3),(ssize_t)3);old=f->er.user_param_override_seq;
 KUNIT_EXPECT_LT(test,user_param_override_store(f->dev,NULL,"3,invalid",9),(ssize_t)0);
 KUNIT_EXPECT_PTR_EQ(test,f->er.user_param_override_seq,old);KUNIT_EXPECT_EQ(test,old[1],2u);
 KUNIT_EXPECT_EQ(test,user_param_override_store(f->dev,NULL,"1,2,3",5),(ssize_t)-EINVAL);
 KUNIT_EXPECT_LT(test,user_param_override_store(f->dev,NULL,"1,256",5),(ssize_t)0);
 KUNIT_EXPECT_LT(test,user_param_override_store(f->dev,NULL,"none-invalid",12),(ssize_t)0);
 KUNIT_EXPECT_PTR_EQ(test,f->er.user_param_override_seq,old);
}
static void host_sequence_and_clear_are_independent(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,user_param_override_store(f->dev,NULL,"1,2",3),(ssize_t)3);
 KUNIT_ASSERT_EQ(test,user_host_param_override_store(f->dev,NULL,"0x3,0x4",7),(ssize_t)7);
 KUNIT_EXPECT_EQ(test,f->er.user_host_param_override_seq[1],4u);
 KUNIT_EXPECT_EQ(test,user_host_param_override_store(f->dev,NULL,"none\n",5),(ssize_t)5);
 KUNIT_EXPECT_PTR_EQ(test,f->er.user_host_param_override_seq,NULL);KUNIT_EXPECT_EQ(test,f->er.user_host_param_override_seq_cnt,(u8)0);
 KUNIT_EXPECT_EQ(test,f->er.user_param_override_seq[1],2u);
}
static void parser_capacity_and_page_output_stay_bounded(struct kunit *test)
{
 struct fixture *f=test->priv;char *input=kunit_kzalloc(test,1024,GFP_KERNEL),*output=kunit_kzalloc(test,PAGE_SIZE+16,GFP_KERNEL);int i;ssize_t size;
 KUNIT_ASSERT_NOT_NULL(test,input);KUNIT_ASSERT_NOT_NULL(test,output);
 for(i=0;i<256;i++){input[i*2]='1';input[i*2+1]=',';}
 KUNIT_EXPECT_EQ(test,user_param_override_store(f->dev,NULL,input,508),(ssize_t)508);
 KUNIT_EXPECT_EQ(test,f->er.user_param_override_seq_cnt,(u8)254);
 memset(output+PAGE_SIZE,0x5a,16);size=user_param_override_show(f->dev,NULL,output);
 KUNIT_EXPECT_GT(test,size,(ssize_t)0);KUNIT_EXPECT_LT(test,size,(ssize_t)PAGE_SIZE);KUNIT_EXPECT_EQ(test,(u8)output[PAGE_SIZE],(u8)0x5a);
 KUNIT_EXPECT_EQ(test,user_param_override_store(f->dev,NULL,input,512),(ssize_t)-E2BIG);
 KUNIT_EXPECT_EQ(test,f->er.user_param_override_seq_cnt,(u8)254);
 KUNIT_EXPECT_EQ(test,user_param_override_store(f->dev,NULL,input,0),(ssize_t)-EINVAL);
 KUNIT_EXPECT_EQ(test,user_param_override_store(f->dev,NULL,input,PAGE_SIZE+1),(ssize_t)-EINVAL);
}
static struct kunit_case cases[]={KUNIT_CASE(version_reads_real_regmap),KUNIT_CASE(failed_version_is_negative),KUNIT_CASE(reset_updates_and_clears_mask),KUNIT_CASE(failed_reset_is_negative),KUNIT_CASE(init_requires_ready_status),KUNIT_CASE(init_propagates_status_read_error),KUNIT_CASE(init_accepts_ready_status),KUNIT_CASE(user_parser_handles_nonterminated_count),KUNIT_CASE(invalid_replacement_preserves_old_sequence),KUNIT_CASE(host_sequence_and_clear_are_independent),KUNIT_CASE(parser_capacity_and_page_output_stay_bounded),{}};
static struct kunit_suite suite={.name="avr-usb-repeater-pmic",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
