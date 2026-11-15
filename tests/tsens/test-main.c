// SPDX-License-Identifier: GPL-2.0
/* Full native-derived production source; real regmap on private scripted RAM.
 * Disable only its platform-driver registration in this test module. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(driver)
#include "../../ports/qcom-tsens/tsens.c"
struct fixture {
 struct device *dev;struct tsens_priv *priv;u32 regs[0x200/4];
 u32 samples[3];unsigned int sample_count,reads;int read_error;
};
static int ram_read(void *context,unsigned int reg,unsigned int *value)
{
 struct fixture *f=context;
 if(reg>=sizeof(f->regs)||reg%4)return -EINVAL;
 if(f->read_error)return f->read_error;
 if(reg>=0xa0&&reg<=0xdc&&f->sample_count){
  unsigned int i=min(f->reads++,f->sample_count-1);*value=f->samples[i];return 0;
 }
 *value=f->regs[reg/4];return 0;
}
static int ram_write(void *context,unsigned int reg,unsigned int value)
{
 struct fixture *f=context;if(reg>=sizeof(f->regs)||reg%4)return -EINVAL;
 f->regs[reg/4]=value;return 0;
}
static const struct regmap_bus ram_bus={.reg_read=ram_read,.reg_write=ram_write};
static const struct regmap_config ram_config={.reg_bits=32,.val_bits=32,.reg_stride=4,.max_register=0x1fc,.fast_io=true};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int i;
 if(!f)return -ENOMEM;test->priv=f;
 f->dev=root_device_register("avr-tsens-ram");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
 f->priv=kunit_kzalloc(test,struct_size(f->priv,sensor,16),GFP_KERNEL);
 if(!f->priv){root_device_unregister(f->dev);return -ENOMEM;}
 f->priv->dev=f->dev;f->priv->num_sensors=16;f->priv->feat=data_tsens_v2.feat;
 f->priv->fields=data_tsens_v2.fields;f->priv->ops=data_tsens_v2.ops;
 f->priv->tm_map=devm_regmap_init(f->dev,&ram_bus,f,&ram_config);
 if(IS_ERR(f->priv->tm_map)){int ret=PTR_ERR(f->priv->tm_map);root_device_unregister(f->dev);return ret;}
 for(i=0;i<MAX_REGFIELDS;i++){
  f->priv->rf[i]=devm_regmap_field_alloc(f->dev,f->priv->tm_map,f->priv->fields[i]);
  if(IS_ERR(f->priv->rf[i])){int ret=PTR_ERR(f->priv->rf[i]);root_device_unregister(f->dev);return ret;}
 }
 for(i=0;i<16;i++){f->priv->sensor[i].priv=f->priv;f->priv->sensor[i].hw_id=i;}
 spin_lock_init(&f->priv->ul_lock);return 0;
}
static void cleanup(struct kunit *test){struct fixture *f=test->priv;root_device_unregister(f->dev);}
static void signed_temperatures_use_exact_v2_layout(struct kunit *test)
{
 struct fixture *f=test->priv;int raw,temp;f->sample_count=1;
 /* Exercise every 12-bit code, including sign boundary, with a valid sample. */
 for(raw=0;raw<4096;raw++){
  f->samples[0]=BIT(21)|raw;f->reads=0;temp=INT_MIN;
  KUNIT_ASSERT_EQ(test,get_temp_tsens_valid(&f->priv->sensor[0],&temp),0);
  KUNIT_ASSERT_EQ(test,temp,(raw>=2048 ? raw-4096 : raw)*100);
 }
}
static void all_sixteen_sensor_offsets_are_independent(struct kunit *test)
{
 struct fixture *f=test->priv;int i,temp;
 for(i=0;i<16;i++)f->regs[(0xa0+4*i)/4]=BIT(21)|(200+i);
 for(i=0;i<16;i++){
  KUNIT_ASSERT_EQ(test,get_temp_tsens_valid(&f->priv->sensor[i],&temp),0);
  KUNIT_EXPECT_EQ(test,temp,(200+i)*100);
 }
}
static void invalid_sample_fallback_requires_a_stable_pair(struct kunit *test)
{
 struct fixture *f=test->priv;int temp=INT_MIN;f->sample_count=3;
 f->samples[0]=250;f->samples[1]=250;f->samples[2]=251;
 KUNIT_ASSERT_EQ(test,get_temp_tsens_valid(&f->priv->sensor[0],&temp),0);KUNIT_EXPECT_EQ(test,temp,25000);
 f->reads=0;f->samples[0]=249;f->samples[1]=251;f->samples[2]=251;
 KUNIT_ASSERT_EQ(test,get_temp_tsens_valid(&f->priv->sensor[0],&temp),0);KUNIT_EXPECT_EQ(test,temp,25100);
 f->reads=0;f->samples[2]=252;temp=12345;
 KUNIT_EXPECT_EQ(test,get_temp_tsens_valid(&f->priv->sensor[0],&temp),-EAGAIN);KUNIT_EXPECT_EQ(test,temp,12345);
}
static void valid_later_sample_overrides_unstable_values(struct kunit *test)
{
 struct fixture *f=test->priv;int temp;f->sample_count=3;
 f->samples[0]=1;f->samples[1]=2;f->samples[2]=BIT(21)|333;
 KUNIT_ASSERT_EQ(test,get_temp_tsens_valid(&f->priv->sensor[0],&temp),0);KUNIT_EXPECT_EQ(test,temp,33300);
}
static void read_errors_propagate_without_temperature_output(struct kunit *test)
{
 struct fixture *f=test->priv;int temp=12345;f->read_error=-EIO;
 KUNIT_EXPECT_EQ(test,get_temp_tsens_valid(&f->priv->sensor[0],&temp),-EIO);KUNIT_EXPECT_EQ(test,temp,12345);
}
static void factory_range_and_threshold_encodings_are_preserved(struct kunit *test)
{
 struct fixture *f=test->priv;struct tsens_sensor *s=&f->priv->sensor[0];
 KUNIT_EXPECT_EQ(test,f->priv->feat->trip_min_temp,-40000);KUNIT_EXPECT_EQ(test,f->priv->feat->trip_max_temp,204000);
 KUNIT_EXPECT_EQ(test,tsens_mC_to_hw(s,-40000),-400);KUNIT_EXPECT_EQ(test,tsens_mC_to_hw(s,145000),1450);
 KUNIT_EXPECT_EQ(test,tsens_hw_to_mC(s,1450),145000);KUNIT_EXPECT_EQ(test,tsens_hw_to_mC(s,0xe70),-40000);
}
static void irq_mask_fields_cover_each_sensor_without_clobber(struct kunit *test)
{
 struct fixture *f=test->priv;int i;
 f->regs[0x10/4]=~0u;f->regs[0x1c/4]=~0u;
 for(i=0;i<16;i++){
  tsens_set_interrupt(f->priv,i,LOWER,true);tsens_set_interrupt(f->priv,i,UPPER,true);tsens_set_interrupt(f->priv,i,CRITICAL,true);
  KUNIT_EXPECT_EQ(test,f->regs[0x10/4],(u32)(~GENMASK(i,0)&~GENMASK(i+16,16)));
  KUNIT_EXPECT_EQ(test,f->regs[0x1c/4],(u32)~GENMASK(i,0));
 }
}
static void irq_state_decodes_signed_thresholds(struct kunit *test)
{
 struct fixture *f=test->priv;struct tsens_irq_data d={0};
 f->regs[0x20/4]=((u32)1450<<12)|0xe70;f->regs[0x60/4]=1500;
 KUNIT_ASSERT_EQ(test,tsens_read_irq_state(f->priv,0,&f->priv->sensor[0],&d),0);
 KUNIT_EXPECT_EQ(test,d.low_thresh,-40000);KUNIT_EXPECT_EQ(test,d.up_thresh,145000);KUNIT_EXPECT_EQ(test,d.crit_thresh,150000);
 f->read_error=-EIO;KUNIT_EXPECT_EQ(test,tsens_read_irq_state(f->priv,0,&f->priv->sensor[0],&d),-EIO);
}
static struct kunit_case cases[]={KUNIT_CASE(signed_temperatures_use_exact_v2_layout),KUNIT_CASE(all_sixteen_sensor_offsets_are_independent),KUNIT_CASE(invalid_sample_fallback_requires_a_stable_pair),KUNIT_CASE(valid_later_sample_overrides_unstable_values),KUNIT_CASE(read_errors_propagate_without_temperature_output),KUNIT_CASE(factory_range_and_threshold_encodings_are_preserved),KUNIT_CASE(irq_mask_fields_cover_each_sensor_without_clobber),KUNIT_CASE(irq_state_decodes_signed_thresholds),{}};
static struct kunit_suite suite={.name="avr-tsens",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
