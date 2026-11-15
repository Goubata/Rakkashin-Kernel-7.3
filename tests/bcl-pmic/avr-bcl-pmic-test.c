// SPDX-License-Identifier: GPL-2.0-only
/* Complete production sources, native thermal/regmap/debugfs/IRQ registries.
 * All bus values and IRQ-chip/wake behavior are private recorded guest data. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#undef module_platform_driver
#define module_platform_driver(...)
#undef pr_fmt
#include "../../ports/qcom-bcl-pmic/qti_bcl_pmic5.c"
#include "../../ports/qcom-bcl-pmic/qti_bcl_stats.c"
struct fixture {
 struct platform_device *pdev;
 struct bcl_device bcl;
 u8 *ram;
 unsigned int calls,fail_at,reads,writes,last_reg,last_value,wake_calls,fail_wake;
 int irq_base,wake_balance[3];bool irqs;
};
static int ram_read(void *ctx,unsigned int reg,unsigned int *val)
{
 struct fixture *f=ctx;f->reads++;f->last_reg=reg;
 if(++f->calls==f->fail_at)return -EIO;
 *val=f->ram[reg];return 0;
}
static int ram_write(void *ctx,unsigned int reg,unsigned int val)
{
 struct fixture *f=ctx;f->writes++;f->last_reg=reg;f->last_value=val;
 if(++f->calls==f->fail_at)return -EIO;
 f->ram[reg]=val;return 0;
}
static const struct regmap_bus bus={.reg_read=ram_read,.reg_write=ram_write};
static const struct regmap_config config={.reg_bits=16,.val_bits=8,.max_register=0xffff,.cache_type=REGCACHE_NONE};
static void irq_nop(struct irq_data *d){}
static int irq_type(struct irq_data *d,unsigned int type){return 0;}
static int irq_wake(struct irq_data *d,unsigned int on)
{
 struct fixture *f=irq_data_get_irq_chip_data(d);int idx=d->irq-f->irq_base;
 if(on && ++f->wake_calls==f->fail_wake)return -EIO;
 f->wake_balance[idx]+=on ? 1 : -1;return 0;
}
static struct irq_chip chip={.name="avr-bcl-private",.irq_mask=irq_nop,.irq_unmask=irq_nop,.irq_ack=irq_nop,.irq_set_type=irq_type,.irq_set_wake=irq_wake};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);struct resource resources[3];int i;
 if(!f)return -ENOMEM;test->priv=f;
 mutex_init(&f->bcl.stats_lock);f->bcl.desc=&pmih010x_data;f->bcl.dig_major=5;f->bcl.fg_bcl_addr=0x4700;f->bcl.ibat_ext_range_factor=10;
 for(i=0;i<BCL_TYPE_MAX;i++){mutex_init(&f->bcl.param[i].state_trans_lock);f->bcl.param[i].type=i;f->bcl.param[i].dev=&f->bcl;f->bcl.param[i].trip_thresh=INT_MAX;}
 for(i=0;i<3;i++)f->bcl.stats[i].bcl_dev=&f->bcl;
 f->ram=kunit_kzalloc(test,65536,GFP_KERNEL);if(!f->ram)return -ENOMEM;
 f->irq_base=irq_alloc_descs(-1,32,3,NUMA_NO_NODE);if(f->irq_base<0)return f->irq_base;f->irqs=true;
 for(i=0;i<3;i++){
  irq_set_chip_data(f->irq_base+i,f);irq_set_chip_and_handler(f->irq_base+i,&chip,handle_simple_irq);irq_clear_status_flags(f->irq_base+i,IRQ_NOREQUEST);
  resources[i]=(struct resource){.start=f->irq_base+i,.end=f->irq_base+i,.flags=IORESOURCE_IRQ,.name=bcl_int_names[BCL_LVL0+i]};
 }
 f->pdev=platform_device_register_simple("avr-bcl-pmic-private",PLATFORM_DEVID_AUTO,resources,3);
 if(IS_ERR(f->pdev)){int ret=PTR_ERR(f->pdev);irq_free_descs(f->irq_base,3);return ret;}
 f->bcl.dev=&f->pdev->dev;platform_set_drvdata(f->pdev,&f->bcl);
 f->bcl.regmap=regmap_init(&f->pdev->dev,&bus,f,&config);
 if(IS_ERR(f->bcl.regmap)){int ret=PTR_ERR(f->bcl.regmap);platform_device_unregister(f->pdev);irq_free_descs(f->irq_base,3);return ret;}
 for(i=0;i<BCL_TYPE_MAX;i++){
  const struct thermal_zone_device_ops *ops;struct bcl_peripheral_data *p=&f->bcl.param[i];
  if(i==BCL_IBAT_LVL0)ops=&(struct thermal_zone_device_ops){.get_temp=bcl_read_ibat};
  else if(i==BCL_VBAT_LVL0)ops=&vbat_tzd_ops;
  else if(i>=BCL_LVL0 && i<=BCL_LVL2){p->ops.get_temp=bcl_read_lbat;p->ops.get_trend=bcl_get_trend;p->status_bit_idx=BIT(i-BCL_LVL0);ops=&p->ops;}
  else continue;
  p->tz_dev=thermal_tripless_zone_device_register("avr-bcl-data",p,ops,NULL);
  if(IS_ERR(p->tz_dev)){p->tz_dev=NULL;return -ENOMEM;}
 }
 f->calls=f->reads=f->writes=0;return 0;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;int i;
 f->fail_at=f->fail_wake=0;bcl_quiesce(&f->bcl);
 for(i=0;i<BCL_TYPE_MAX;i++)if(f->bcl.param[i].tz_dev)thermal_zone_device_unregister(f->bcl.param[i].tz_dev);
 regmap_exit(f->bcl.regmap);platform_device_unregister(f->pdev);
 for(i=0;i<3;i++){KUNIT_EXPECT_EQ(test,f->wake_balance[i],0);irq_set_chip_data(f->irq_base+i,NULL);}
 if(f->irqs)irq_free_descs(f->irq_base,3);
}
static void sample(struct fixture *f,int reg,u16 value){put_unaligned_le16(value,f->ram+0x4700+reg);}
static void voltage_gen5_samples(struct kunit *test)
{
 struct fixture *f=test->priv;int i,value;const u16 raw[]={1,127,128,255,256,0x1234,0x7ffe,0xffff};
 const int expected[]={0,24,24,49,49,907,6377,12755};
 for(i=0;i<ARRAY_SIZE(raw);i++){sample(f,BCL_VBAT_READ,raw[i]);KUNIT_ASSERT_EQ(test,bcl_read_vbat_tz(f->bcl.param[BCL_VBAT_LVL0].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,expected[i]);}
}
static void signed_current_gen5(struct kunit *test)
{
 struct fixture *f=test->priv;int i,value;const u16 raw[]={0x100,0x7fff,0x8000,0xffff};const int expected[]={40,5120,-5120,0};
 for(i=0;i<4;i++){sample(f,BCL_IBAT_READ,raw[i]);KUNIT_ASSERT_EQ(test,bcl_read_ibat(f->bcl.param[0].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,expected[i]);}
}
static void cached_no_reading(struct kunit *test)
{
 struct fixture *f=test->priv;int value;
 sample(f,BCL_VBAT_READ,0x1234);KUNIT_ASSERT_EQ(test,bcl_read_vbat_tz(f->bcl.param[2].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,907);
 sample(f,BCL_VBAT_READ,0x7fff);KUNIT_ASSERT_EQ(test,bcl_read_vbat_tz(f->bcl.param[2].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,907);
 sample(f,BCL_IBAT_READ,0x100);KUNIT_ASSERT_EQ(test,bcl_read_ibat(f->bcl.param[0].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,40);
 sample(f,BCL_IBAT_READ,0);KUNIT_ASSERT_EQ(test,bcl_read_ibat(f->bcl.param[0].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,40);
}
static void legacy_signed_and_voltage(struct kunit *test)
{
 struct fixture *f=test->priv;int value;f->bcl.dig_major=3;
 sample(f,BCL_IBAT_READ,0x80);KUNIT_ASSERT_EQ(test,bcl_read_ibat(f->bcl.param[0].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,-10000);
 sample(f,BCL_VBAT_READ,50);KUNIT_ASSERT_EQ(test,bcl_read_vbat_tz(f->bcl.param[2].tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,2491);
}
static void read_errors_preserve_output(struct kunit *test)
{
 struct fixture *f=test->priv;int n,value;
 for(n=1;n<=2;n++){
  f->calls=0;f->fail_at=n;value=777;KUNIT_EXPECT_EQ(test,bcl_read_ibat(f->bcl.param[0].tz_dev,&value),-EIO);KUNIT_EXPECT_EQ(test,value,777);
  f->calls=0;value=777;KUNIT_EXPECT_EQ(test,bcl_read_vbat_tz(f->bcl.param[2].tz_dev,&value),-EIO);KUNIT_EXPECT_EQ(test,value,777);
 }
}
static void bpm_snapshot_endianness_and_scale(struct kunit *test)
{
 struct fixture *f=test->priv;struct bcl_bpm_stats out;
 sample(f,BPM_MAX_IBAT_OFFSET,0x8000);sample(f,BPM_SYNC_VBAT_OFFSET,0x1234);sample(f,BPM_MIN_VBAT_OFFSET,0x100);sample(f,BPM_SYNC_IBAT_OFFSET,0x7fff);
 f->ram[0x4700+BCL_LVL0_CNT_OFFSET]=1;f->ram[0x4700+BCL_LVL1_CNT_OFFSET]=2;f->ram[0x4700+BCL_LVL2_CNT_OFFSET]=3;
 KUNIT_ASSERT_EQ(test,get_bpm_stats(&f->bcl,&out),0);KUNIT_EXPECT_EQ(test,out.max_ibat,-2000);KUNIT_EXPECT_EQ(test,out.sync_vbat,907);KUNIT_EXPECT_EQ(test,out.min_vbat,49);KUNIT_EXPECT_EQ(test,out.sync_ibat,1999);
 KUNIT_EXPECT_EQ(test,out.lvl0_cnt,1);KUNIT_EXPECT_EQ(test,out.lvl1_cnt,2);KUNIT_EXPECT_EQ(test,out.lvl2_cnt,3);KUNIT_EXPECT_EQ(test,f->ram[0x4700+BPM_EN_OFFSET],BPM_CLR);
}
static void bpm_all_bus_failures_are_transactional(struct kunit *test)
{
 struct fixture *f=test->priv;struct bcl_bpm_stats out,guard;int n;
 memset(&guard,0xa5,sizeof(guard));f->bcl.last_bpm_read_ts=123;
 for(n=1;n<=13;n++){
  out=guard;f->calls=f->reads=f->writes=0;f->fail_at=n;
  KUNIT_EXPECT_EQ(test,get_bpm_stats(&f->bcl,&out),-EIO);KUNIT_EXPECT_MEMEQ(test,&out,&guard,sizeof(out));KUNIT_EXPECT_EQ(test,f->bcl.last_bpm_read_ts,123ull);
  if(n>1){KUNIT_EXPECT_EQ(test,f->last_reg,0x4700u+BPM_EN_OFFSET);KUNIT_EXPECT_EQ(test,f->last_value,(unsigned int)BPM_CLR);}
 }
}
static void voltage_thresholds_and_bounds(struct kunit *test)
{
 struct fixture *f=test->priv;int value=0;unsigned int calls;
 f->bcl.no_bit_shift=true;KUNIT_ASSERT_EQ(test,bcl_set_adc_value(&f->bcl,0,2800,&value),0);KUNIT_EXPECT_EQ(test,value,56);KUNIT_EXPECT_EQ(test,f->ram[0x4748],56);
 KUNIT_ASSERT_EQ(test,bcl_set_adc_value(&f->bcl,1,2600,&value),0);KUNIT_EXPECT_EQ(test,value,14);KUNIT_EXPECT_EQ(test,f->ram[0x4749],14);
 calls=f->calls;KUNIT_EXPECT_EQ(test,bcl_set_adc_value(&f->bcl,-1,2800,&value),-EINVAL);KUNIT_EXPECT_EQ(test,bcl_set_adc_value(&f->bcl,3,2800,&value),-EINVAL);KUNIT_EXPECT_EQ(test,bcl_set_adc_value(&f->bcl,0,2249,&value),-EINVAL);KUNIT_EXPECT_EQ(test,bcl_set_adc_value(&f->bcl,0,3601,&value),-EINVAL);KUNIT_EXPECT_EQ(test,f->calls,calls);
}
static void voltage_threshold_write_error(struct kunit *test)
{
 struct fixture *f=test->priv;int value;f->fail_at=1;KUNIT_EXPECT_EQ(test,bcl_set_adc_value(&f->bcl,0,2800,&value),-EIO);KUNIT_EXPECT_EQ(test,f->ram[0x4748],0);
}
static void ccm_clamps_before_narrowing(struct kunit *test)
{
 KUNIT_EXPECT_EQ(test,convert_ibat_to_ccm_val(INT_MAX,14,800,100),14);KUNIT_EXPECT_EQ(test,convert_ibat_to_ccm_val(INT_MIN,14,800,100),0);KUNIT_EXPECT_EQ(test,convert_ibat_to_ccm_val(800,14,800,100),0);KUNIT_EXPECT_EQ(test,convert_ibat_to_ccm_val(2200,14,800,100),14);KUNIT_EXPECT_EQ(test,convert_ibat_to_ccm_val(INT_MAX,16,900,150),16);
}
static void current_threshold_native_io(struct kunit *test)
{
 struct fixture *f=test->priv;struct bcl_peripheral_data *p=&f->bcl.param[0];f->bcl.no_bit_shift=true;
 KUNIT_ASSERT_EQ(test,bcl_set_ibat(p->tz_dev,0,11000),0);KUNIT_EXPECT_EQ(test,f->ram[0x4700+BCL_IBAT_HIGH],70);KUNIT_EXPECT_EQ(test,p->trip_thresh,11000l);
 f->calls=0;f->fail_at=1;KUNIT_EXPECT_EQ(test,bcl_set_ibat(p->tz_dev,0,11500),-EIO);KUNIT_EXPECT_EQ(test,p->trip_thresh,11000l);
}
static void logical_level_metrics_and_trend(struct kunit *test)
{
 struct fixture *f=test->priv;int i,value;enum thermal_trend trend;f->ram[0x4708]=7;
 for(i=0;i<3;i++){struct bcl_peripheral_data *p=&f->bcl.param[5+i];KUNIT_ASSERT_EQ(test,bcl_read_lbat(p->tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,BIT(i));KUNIT_ASSERT_EQ(test,bcl_get_trend(p->tz_dev,NULL,&trend),0);KUNIT_EXPECT_EQ(test,trend,THERMAL_TREND_RAISING);}
 f->ram[0x4708]=0;KUNIT_ASSERT_EQ(test,bcl_read_lbat(f->bcl.param[5].tz_dev,&value),0);bcl_get_trend(f->bcl.param[5].tz_dev,NULL,&trend);KUNIT_EXPECT_EQ(test,trend,THERMAL_TREND_DROPPING);
}
static void revision_read_errors_propagate(struct kunit *test)
{
 struct fixture *f=test->priv;int n;f->ram[0x4701]=5;f->ram[0x4700]=1;f->ram[0x470e]=1;f->ram[0x470f]=2;
 for(n=1;n<=4;n++){f->calls=0;f->fail_at=n;KUNIT_EXPECT_EQ(test,bcl_version_init_and_check(&f->bcl),-EIO);}
 f->fail_at=0;KUNIT_EXPECT_EQ(test,bcl_version_init_and_check(&f->bcl),0);KUNIT_EXPECT_EQ(test,f->bcl.dig_major,5);KUNIT_EXPECT_EQ(test,f->bcl.bcl_type,2);
}
static void stopped_callbacks_do_not_access_bus(struct kunit *test)
{
 struct fixture *f=test->priv;int value=777;WRITE_ONCE(f->bcl.stopping,true);
 KUNIT_EXPECT_EQ(test,bcl_read_ibat(f->bcl.param[0].tz_dev,&value),-ENODEV);KUNIT_EXPECT_EQ(test,bcl_read_vbat_tz(f->bcl.param[2].tz_dev,&value),-ENODEV);KUNIT_EXPECT_EQ(test,bcl_read_lbat(f->bcl.param[5].tz_dev,&value),-ENODEV);KUNIT_EXPECT_EQ(test,value,777);
 KUNIT_EXPECT_EQ(test,bcl_set_ibat(f->bcl.param[0].tz_dev,0,11000),-ENODEV);KUNIT_EXPECT_EQ(test,bcl_set_lbat(f->bcl.param[5].tz_dev,0,1),-ENODEV);KUNIT_EXPECT_EQ(test,f->calls,0u);
}
static irqreturn_t dummy_irq(int irq,void *data){return IRQ_HANDLED;}
static void acquire_irqs(struct kunit *test)
{
 struct fixture *f=test->priv;int i;
 for(i=0;i<3;i++)KUNIT_ASSERT_EQ(test,bcl_fetch_trip(f->pdev,BCL_LVL0+i,&f->bcl.param[5+i],bcl_handle_irq),0);
}
static void native_irq_wake_remove_ownership(struct kunit *test)
{
 struct fixture *f=test->priv;int i,ret;acquire_irqs(test);
 for(i=0;i<3;i++){KUNIT_ASSERT_EQ(test,bcl_set_lbat(f->bcl.param[5+i].tz_dev,0,1),0);KUNIT_EXPECT_EQ(test,f->wake_balance[i],1);}
 bcl_quiesce(&f->bcl);bcl_quiesce(&f->bcl);
 for(i=0;i<3;i++){KUNIT_EXPECT_EQ(test,f->wake_balance[i],0);ret=request_irq(f->irq_base+i,dummy_irq,0,"avr-bcl-reclaim",f);KUNIT_EXPECT_EQ(test,ret,0);if(!ret)free_irq(f->irq_base+i,f);}
}
static void native_irq_wake_error_does_not_arm(struct kunit *test)
{
 struct fixture *f=test->priv;acquire_irqs(test);f->fail_wake=1;
 KUNIT_EXPECT_EQ(test,bcl_set_lbat(f->bcl.param[5].tz_dev,0,1),-EIO);KUNIT_EXPECT_FALSE(test,f->bcl.param[5].irq_enabled);KUNIT_EXPECT_FALSE(test,f->bcl.param[5].irq_wake);
}
static void freeze_restore_keeps_native_irq_actions(struct kunit *test)
{
 struct fixture *f=test->priv;int i;acquire_irqs(test);
 for(i=0;i<3;i++)KUNIT_ASSERT_EQ(test,bcl_set_lbat(f->bcl.param[5+i].tz_dev,0,1),0);
 KUNIT_ASSERT_EQ(test,bcl_freeze(&f->pdev->dev),0);KUNIT_ASSERT_EQ(test,bcl_freeze(&f->pdev->dev),0);
 for(i=0;i<3;i++){KUNIT_EXPECT_TRUE(test,f->bcl.param[5+i].irq_requested);KUNIT_EXPECT_FALSE(test,f->bcl.param[5+i].irq_enabled);KUNIT_EXPECT_EQ(test,f->wake_balance[i],0);}
 KUNIT_ASSERT_EQ(test,bcl_restore(&f->pdev->dev),0);
 for(i=0;i<3;i++){KUNIT_EXPECT_TRUE(test,f->bcl.param[5+i].irq_enabled);KUNIT_EXPECT_EQ(test,f->wake_balance[i],1);}
}
static void restore_wake_failure_unwinds(struct kunit *test)
{
 struct fixture *f=test->priv;int i;acquire_irqs(test);
 for(i=0;i<3;i++)KUNIT_ASSERT_EQ(test,bcl_set_lbat(f->bcl.param[5+i].tz_dev,0,1),0);
 KUNIT_ASSERT_EQ(test,bcl_freeze(&f->pdev->dev),0);f->wake_calls=0;f->fail_wake=2;
 KUNIT_EXPECT_EQ(test,bcl_restore(&f->pdev->dev),-EIO);KUNIT_EXPECT_TRUE(test,f->bcl.frozen);
 for(i=0;i<3;i++){KUNIT_EXPECT_FALSE(test,f->bcl.param[5+i].irq_enabled);KUNIT_EXPECT_EQ(test,f->wake_balance[i],0);}
 f->fail_wake=0;KUNIT_EXPECT_EQ(test,bcl_restore(&f->pdev->dev),0);
}
static void debugfs_subtree_ownership(struct kunit *test)
{
 struct fixture *f=test->priv;struct bcl_device *other=kunit_kzalloc(test,sizeof(*other),GFP_KERNEL);struct dentry *found;KUNIT_ASSERT_NOT_NULL(test,other);mutex_init(&other->stats_lock);
 bcl_stats_init("avr-pmic-a",&f->bcl,3);bcl_stats_init("avr-pmic-b",other,3);KUNIT_EXPECT_EQ(test,bcl_stats_users,2u);
 bcl_stats_remove(&f->bcl);KUNIT_EXPECT_EQ(test,bcl_stats_users,1u);found=debugfs_lookup("avr-pmic-b",bcl_stats_parent);KUNIT_EXPECT_NOT_NULL(test,found);if(found)dput(found);
 bcl_stats_remove(&f->bcl);bcl_stats_remove(other);KUNIT_EXPECT_EQ(test,bcl_stats_users,0u);KUNIT_EXPECT_PTR_EQ(test,bcl_stats_parent,NULL);
}
static void bpm_reset_native_error(struct kunit *test)
{
 struct fixture *f=test->priv;f->bcl.last_bpm_reset_ts=123;f->fail_at=1;
 KUNIT_EXPECT_EQ(test,bpm_reset(&f->bcl,1),-EIO);KUNIT_EXPECT_EQ(test,f->bcl.last_bpm_reset_ts,123ull);f->fail_at=0;
 KUNIT_EXPECT_EQ(test,bpm_reset(&f->bcl,1),0);KUNIT_EXPECT_EQ(test,f->ram[0x4700+BPM_CLR_OFFSET],1);
}
static void frozen_threshold_updates_request_retry(struct kunit *test)
{
 struct fixture *f=test->priv;WRITE_ONCE(f->bcl.frozen,true);
 KUNIT_EXPECT_EQ(test,bcl_set_ibat(f->bcl.param[0].tz_dev,0,INT_MAX),-EBUSY);
 KUNIT_EXPECT_EQ(test,bcl_set_ibat(f->bcl.param[0].tz_dev,0,11000),-EBUSY);
 KUNIT_EXPECT_EQ(test,bcl_set_lbat(f->bcl.param[5].tz_dev,0,INT_MAX),-EBUSY);
 KUNIT_EXPECT_EQ(test,bcl_set_lbat(f->bcl.param[5].tz_dev,0,1),-EBUSY);
 KUNIT_EXPECT_EQ(test,f->calls,0u);
}
static void peer_threshold_error_reaches_caller(struct kunit *test)
{
 struct fixture *f=test->priv;
 struct bcl_device *peer=kunit_kzalloc(test,sizeof(*peer),GFP_KERNEL);
 struct thermal_trip trip={.temperature=2800,.hysteresis=100,.type=THERMAL_TRIP_PASSIVE};
 struct thermal_zone_device *tz;int ret;
 KUNIT_ASSERT_NOT_NULL(test,peer);
 peer->desc=&pm8550_data;peer->regmap=f->bcl.regmap;peer->fg_bcl_addr=0x4900;peer->nb.notifier_call=bcl_config_vph_cb;
 tz=thermal_zone_device_register_with_trips("avr-bcl-broadcast",&trip,1,&f->bcl.param[2],&vbat_tzd_ops,NULL,0,0);
 KUNIT_ASSERT_FALSE(test,IS_ERR(tz));
 ret=bcl_pmic5_notifier_register(&peer->nb);KUNIT_EXPECT_EQ(test,ret,0);
 if(!ret){
  f->calls=0;f->fail_at=2;
  KUNIT_EXPECT_EQ(test,bcl_write_vbat_tz(tz,&tz->trips[0].trip,2800),-EIO);
  KUNIT_EXPECT_NE(test,f->ram[0x4748],0);KUNIT_EXPECT_EQ(test,f->ram[0x4947],0);
  f->fail_at=0;KUNIT_EXPECT_EQ(test,bcl_write_vbat_tz(tz,&tz->trips[0].trip,2800),0);KUNIT_EXPECT_EQ(test,f->ram[0x4947],22);
  bcl_pmic5_notifier_unregister(&peer->nb);
 }
 thermal_zone_device_unregister(tz);
}
static struct kunit_case cases[]={
 KUNIT_CASE(voltage_gen5_samples),KUNIT_CASE(signed_current_gen5),KUNIT_CASE(cached_no_reading),KUNIT_CASE(legacy_signed_and_voltage),KUNIT_CASE(read_errors_preserve_output),
 KUNIT_CASE(bpm_snapshot_endianness_and_scale),KUNIT_CASE(bpm_all_bus_failures_are_transactional),KUNIT_CASE(voltage_thresholds_and_bounds),KUNIT_CASE(voltage_threshold_write_error),
 KUNIT_CASE(ccm_clamps_before_narrowing),KUNIT_CASE(current_threshold_native_io),KUNIT_CASE(logical_level_metrics_and_trend),KUNIT_CASE(revision_read_errors_propagate),KUNIT_CASE(stopped_callbacks_do_not_access_bus),
 KUNIT_CASE(native_irq_wake_remove_ownership),KUNIT_CASE(native_irq_wake_error_does_not_arm),KUNIT_CASE(freeze_restore_keeps_native_irq_actions),KUNIT_CASE(restore_wake_failure_unwinds),KUNIT_CASE(debugfs_subtree_ownership),KUNIT_CASE(bpm_reset_native_error),KUNIT_CASE(frozen_threshold_updates_request_retry),KUNIT_CASE(peer_threshold_error_reaches_caller),{}
};
static struct kunit_suite suite={.name="avr-bcl-pmic",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
