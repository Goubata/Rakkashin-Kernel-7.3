// SPDX-License-Identifier: GPL-2.0-only
/* Native driver binding/OF/NVMEM/thermal/IRQ/devres with private RAM hardware.
 * Normal operations use native APIs. Only selected zone errors are injected. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/nvmem-provider.h>
#include <linux/irq.h>
#include <linux/irqdesc.h>
#include <linux/delay.h>
static struct thermal_zone_device *probe_zone(struct device *, int, void *, const struct thermal_zone_device_ops *);
#undef module_platform_driver
#define module_platform_driver(...)
#define devm_thermal_of_zone_register probe_zone
#undef pr_fmt
#include "../../ports/qcom-bcl-pmic/qti_bcl_pmic5.c"
#undef devm_thermal_of_zone_register
#include "../../ports/qcom-bcl-pmic/qti_bcl_stats.c"
struct fixture {
 struct platform_device *parent,*child;
 struct regmap *map;
 struct nvmem_device *nvmem;
 struct device_node *sensor,*nvmem_node;
 u8 *ram,cell;
 int irq_base,wake_balance[3],probe_ret,probe_calls,zone_error_id;
 int fail_read_reg,fail_write_reg,nvmem_fail_read,nvmem_reads;
 bool driver_registered,provider_registered,skip_map;
 int provider_ret;
};
static struct fixture *backend;
static const char *zone_names[]={"avr-probe-current0","avr-probe-current1","pmih010x-2s-ibat0","pmih010x-2s-ibat1","avr-probe-voltage","avr-probe-level0","avr-probe-level1","avr-probe-level2"};
static int ram_read(void *ctx,unsigned int reg,unsigned int *val)
{
 struct fixture *f=ctx;
 if(reg==f->fail_read_reg)return -EIO;
 *val=READ_ONCE(f->ram[reg]);return 0;
}
static int ram_write(void *ctx,unsigned int reg,unsigned int val)
{
 struct fixture *f=ctx;
 if(reg==f->fail_write_reg)return -EIO;
 WRITE_ONCE(f->ram[reg],val);return 0;
}
static const struct regmap_bus bus={.reg_read=ram_read,.reg_write=ram_write};
static const struct regmap_config map_config={.reg_bits=16,.val_bits=8,.max_register=0xffff,.cache_type=REGCACHE_NONE};
static int cell_read(void *data,unsigned int off,void *value,size_t bytes)
{
 struct fixture *f=data;
 if(++f->nvmem_reads==f->nvmem_fail_read)return -EIO;
 if(off!=0x5d || bytes!=1)return -ERANGE;
 *(u8 *)value=READ_ONCE(f->cell);return 0;
}
static void irq_nop(struct irq_data *d){}
static int irq_type(struct irq_data *d,unsigned int type){return 0;}
static int irq_wake(struct irq_data *d,unsigned int on)
{
 struct fixture *f=irq_data_get_irq_chip_data(d);
 f->wake_balance[d->irq-f->irq_base]+=on ? 1 : -1;return 0;
}
static struct irq_chip chip={.name="avr-bcl-probe-private",.irq_mask=irq_nop,.irq_unmask=irq_nop,.irq_ack=irq_nop,.irq_set_type=irq_type,.irq_set_wake=irq_wake};
static const struct of_device_id probe_match[]={
 {.compatible="avr,bcl-pmic-private",.data=&pmih010x_data},{}
};
static const struct of_device_id peer_match[]={
 {.compatible="avr,bcl-pmic-private",.data=&pm8550_data},{}
};
static struct thermal_zone_device *probe_zone(struct device *dev,int id,void *data,const struct thermal_zone_device_ops *ops)
{
 if(id==backend->zone_error_id)return ERR_PTR(-EIO);
 return devm_thermal_of_zone_register(dev,id,data,ops);
}
static int record_probe(struct platform_device *pdev)
{
 int ret=bcl_probe(pdev);backend->probe_ret=ret;backend->probe_calls++;return ret;
}
static int provider_probe(struct platform_device *pdev)
{
 struct fixture *f=backend;
 struct nvmem_config cfg={.dev=&pdev->dev,.of_node=f->nvmem_node,.owner=THIS_MODULE,.name="avr-bcl-probe-nvmem",.id=NVMEM_DEVID_AUTO,.add_legacy_fixed_of_cells=true,.size=256,.word_size=1,.stride=1,.read_only=true,.reg_read=cell_read,.priv=f};
 if(!f->skip_map){
  f->map=regmap_init(&pdev->dev,&bus,f,&map_config);
  if(IS_ERR(f->map)){int ret=PTR_ERR(f->map);f->map=NULL;f->provider_ret=ret;return ret;}
 }
 f->nvmem=nvmem_register(&cfg);
 if(IS_ERR(f->nvmem)){
  int ret=PTR_ERR(f->nvmem);f->nvmem=NULL;
  if(f->map){regmap_exit(f->map);f->map=NULL;}
  f->provider_ret=ret;return ret;
 }
 f->provider_ret=0;return 0;
}
static void provider_remove(struct platform_device *pdev)
{
 if(backend->nvmem){nvmem_unregister(backend->nvmem);backend->nvmem=NULL;}
 if(backend->map){regmap_exit(backend->map);backend->map=NULL;}
}
static const struct of_device_id provider_match[]={ {.compatible="avr,bcl-sdam-private"},{} };
static struct platform_driver provider_driver={.probe=provider_probe,.remove=provider_remove,.driver={.name="avr-bcl-regmap-parent",.of_match_table=provider_match}};
static int publish_nvmem(struct fixture *f)
{
 int ret=platform_driver_register(&provider_driver);
 if(ret)return ret;
 f->provider_registered=true;return f->provider_ret;
}
static int start_child(struct fixture *f)
{
 struct resource res[3];int i,ret;
 f->probe_ret=INT_MIN;
 f->child=platform_device_alloc("avr-bcl-pmic-probe",PLATFORM_DEVID_AUTO);
 if(!f->child)return -ENOMEM;
 f->child->dev.parent=&f->parent->dev;
 device_set_node(&f->child->dev,of_fwnode_handle(of_node_get(f->sensor)));
 for(i=0;i<3;i++)res[i]=(struct resource){.name=bcl_int_names[BCL_LVL0+i],.flags=IORESOURCE_IRQ,.start=f->irq_base+i,.end=f->irq_base+i};
 ret=platform_device_add_resources(f->child,res,3);
 if(!ret)ret=platform_device_add(f->child);
 if(ret){platform_device_put(f->child);f->child=NULL;}
 wait_for_device_probe();return ret;
}
static void stop_child(struct fixture *f)
{
 if(f->child){platform_device_unregister(f->child);f->child=NULL;}
}
static void release(struct fixture *f)
{
 int i;stop_child(f);
 if(f->driver_registered){platform_driver_unregister(&bcl_driver);f->driver_registered=false;}
 wait_for_device_probe();
 if(f->provider_registered){platform_driver_unregister(&provider_driver);f->provider_registered=false;}
 if(f->map)regmap_exit(f->map);
 if(f->parent)platform_device_unregister(f->parent);
 if(f->irq_base>=0){for(i=0;i<3;i++)irq_set_chip_data(f->irq_base+i,NULL);irq_free_descs(f->irq_base,3);}
 of_node_put(f->sensor);of_node_put(f->nvmem_node);backend=NULL;
}
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int i,ret;
 if(!f)return -ENOMEM;test->priv=f;backend=f;f->irq_base=-1;f->zone_error_id=-1;f->fail_read_reg=f->fail_write_reg=-1;f->probe_ret=INT_MIN;f->provider_ret=INT_MIN;
 f->sensor=of_find_node_by_path("/avr-bcl-probe-bus/bcl@4700");f->nvmem_node=of_find_node_by_path("/avr-bcl-probe-bus/nvmem-test");
 if(!f->sensor || !f->nvmem_node){ret=-ENODEV;goto fail;}
 f->ram=kunit_kzalloc(test,65536,GFP_KERNEL);if(!f->ram){ret=-ENOMEM;goto fail;}
 f->ram[0x4701]=5;f->ram[0x4700]=1;f->ram[0x470e]=1;f->ram[0x470f]=0;
 f->irq_base=irq_alloc_descs(-1,32,3,NUMA_NO_NODE);if(f->irq_base<0){ret=f->irq_base;goto fail;}
 for(i=0;i<3;i++){irq_set_chip_data(f->irq_base+i,f);irq_set_chip_and_handler(f->irq_base+i,&chip,handle_simple_irq);irq_clear_status_flags(f->irq_base+i,IRQ_NOREQUEST);}
 f->parent=platform_device_alloc("avr-bcl-regmap-parent",PLATFORM_DEVID_AUTO);
 if(!f->parent){ret=-ENOMEM;goto fail;}
 device_set_node(&f->parent->dev,of_fwnode_handle(of_node_get(f->nvmem_node)));
 ret=platform_device_add(f->parent);
 if(ret){platform_device_put(f->parent);f->parent=NULL;goto fail;}
 bcl_driver.driver.name="avr-bcl-pmic-probe";bcl_driver.driver.of_match_table=probe_match;bcl_driver.probe=record_probe;
 ret=platform_driver_register(&bcl_driver);if(ret)goto fail;f->driver_registered=true;
 return 0;
fail:release(f);return ret;
}
static irqreturn_t dummy_irq(int irq,void *data){return IRQ_HANDLED;}
static void assert_empty(struct kunit *test)
{
 struct fixture *f=test->priv;int i,ret;
 if(f->child)KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->child),NULL);
 for(i=0;i<ARRAY_SIZE(zone_names);i++)KUNIT_EXPECT_TRUE(test,IS_ERR(thermal_zone_get_zone_by_name(zone_names[i])));
 KUNIT_EXPECT_EQ(test,bcl_stats_users,0u);KUNIT_EXPECT_PTR_EQ(test,bcl_stats_parent,NULL);
 KUNIT_EXPECT_PTR_EQ(test,bcl_pmic5_notifier.head,NULL);
 for(i=0;i<3;i++){
  KUNIT_EXPECT_EQ(test,f->wake_balance[i],0);
  ret=request_irq(f->irq_base+i,dummy_irq,0,"avr-bcl-probe-reclaim",f);KUNIT_EXPECT_EQ(test,ret,0);if(!ret)free_irq(f->irq_base+i,f);
 }
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;
 f->fail_read_reg=f->fail_write_reg=-1;f->nvmem_fail_read=0;stop_child(f);assert_empty(test);release(f);
}
static void assert_bound(struct kunit *test,bool alternate)
{
 struct fixture *f=test->priv;struct bcl_device *bcl=platform_get_drvdata(f->child);int i,count=0;
 KUNIT_ASSERT_GT(test,f->probe_calls,0);KUNIT_ASSERT_EQ(test,f->probe_ret,0);KUNIT_ASSERT_NOT_NULL(test,bcl);
 KUNIT_EXPECT_EQ(test,bcl->fg_bcl_addr,0x4700);KUNIT_EXPECT_EQ(test,bcl->dig_major,5);KUNIT_EXPECT_EQ(test,bcl->ibat_ext_range_factor,alternate ? 20u : 10u);
 for(i=0;i<BCL_TYPE_MAX;i++){
  bool wanted=i==2 || (i>=5 && i<=7) || (alternate ? i>=8 : i<=1);
  KUNIT_EXPECT_EQ(test,!!bcl->param[i].tz_dev,wanted);if(bcl->param[i].tz_dev)count++;
 }
 KUNIT_EXPECT_EQ(test,count,6);KUNIT_EXPECT_TRUE(test,bcl->enable_bpm);KUNIT_EXPECT_NOT_NULL(test,bcl->ipc_log);KUNIT_EXPECT_TRUE(test,bcl->stats_registered);
 for(i=0;i<3;i++){KUNIT_EXPECT_TRUE(test,bcl->param[5+i].irq_requested);KUNIT_EXPECT_TRUE(test,bcl->param[5+i].irq_enabled);KUNIT_EXPECT_EQ(test,f->wake_balance[i],1);}
}
static void native_probe_standard_pair(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);assert_bound(test,false);
}
static void native_probe_2s_pair(struct kunit *test)
{
 struct fixture *f=test->priv;f->cell=BIT(5);KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);assert_bound(test,true);
}
static void late_nvmem_supplier_bind_retries(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_PTR_EQ(test,f->child->dev.driver,NULL);assert_empty(test);
 KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);wait_for_device_probe();assert_bound(test,false);
}
static void first_nvmem_read_error_propagates(struct kunit *test)
{
 struct fixture *f=test->priv;f->nvmem_fail_read=1;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_EQ(test,f->probe_ret,-EIO);assert_empty(test);
}
static void second_nvmem_read_unwinds_voltage_zone(struct kunit *test)
{
 struct fixture *f=test->priv;f->nvmem_fail_read=2;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_EQ(test,f->probe_ret,-EIO);KUNIT_EXPECT_EQ(test,f->nvmem_reads,2);assert_empty(test);
}
static void revision_failures_leave_no_resources(struct kunit *test)
{
 struct fixture *f=test->priv;int regs[]={0x4701,0x4700,0x470e,0x470f};int i;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);
 for(i=0;i<ARRAY_SIZE(regs);i++){f->fail_read_reg=regs[i];KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_EQ(test,f->probe_ret,-EIO);assert_empty(test);stop_child(f);}
}
static void zone_failures_unwind_owned_irqs(struct kunit *test)
{
 struct fixture *f=test->priv;int i;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);
 for(i=5;i<=7;i++){f->zone_error_id=i;KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_EQ(test,f->probe_ret,-EIO);assert_empty(test);stop_child(f);}
}
static void monitor_enable_error_unwinds_all_zones(struct kunit *test)
{
 struct fixture *f=test->priv;f->fail_write_reg=0x4746;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_EQ(test,f->probe_ret,-EIO);assert_empty(test);
}
static void bpm_error_unwinds_registered_notifier(struct kunit *test)
{
 struct fixture *f=test->priv;f->fail_write_reg=0x47a0;bcl_driver.driver.of_match_table=peer_match;
 KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_EQ(test,f->probe_ret,-EIO);assert_empty(test);
}
static void native_release_and_rebind(struct kunit *test)
{
 struct fixture *f=test->priv;int i;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);assert_bound(test,false);
 for(i=0;i<3;i++){device_release_driver(&f->child->dev);assert_empty(test);KUNIT_ASSERT_EQ(test,device_driver_attach(&bcl_driver.driver,&f->child->dev),0);assert_bound(test,false);}
}
static void native_irq_thread_and_level_rearm(struct kunit *test)
{
 struct fixture *f=test->priv;struct bcl_device *bcl;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);assert_bound(test,false);
 bcl=platform_get_drvdata(f->child);KUNIT_ASSERT_NOT_NULL(test,bcl);WRITE_ONCE(f->ram[0x4708],1);
 KUNIT_ASSERT_EQ(test,generic_handle_irq_safe(f->irq_base),0);synchronize_irq(f->irq_base);
 KUNIT_EXPECT_EQ(test,atomic_long_read(&bcl->irq_events[0]),1l);KUNIT_EXPECT_EQ(test,bcl->param[5].last_val,1);KUNIT_EXPECT_FALSE(test,bcl->param[5].irq_enabled);
 WRITE_ONCE(f->ram[0x4708],0);thermal_zone_device_update(bcl->param[5].tz_dev,THERMAL_DEVICE_UP);
 KUNIT_EXPECT_TRUE(test,bcl->param[5].irq_enabled);KUNIT_EXPECT_EQ(test,f->wake_balance[0],1);KUNIT_EXPECT_EQ(test,bcl->stats[0].counter,1u);
}
static void missing_parent_regmap_defers(struct kunit *test)
{
 struct fixture *f=test->priv;f->skip_map=true;KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);KUNIT_EXPECT_EQ(test,f->probe_ret,-EPROBE_DEFER);assert_empty(test);
}
static int unavailable_temperature(struct thermal_zone_device *tz,int *value){return -ENODEV;}
static void original_2s_names_exceed_native_limit(struct kunit *test)
{
 struct thermal_zone_device *tz;
 tz=thermal_tripless_zone_device_register("pmih010x-2s-ibat-lvl0",NULL,&(struct thermal_zone_device_ops){.get_temp=unavailable_temperature},NULL);
 KUNIT_EXPECT_TRUE(test,IS_ERR(tz));if(IS_ERR(tz))KUNIT_EXPECT_EQ(test,PTR_ERR(tz),-EINVAL);else thermal_zone_device_unregister(tz);
 tz=thermal_tripless_zone_device_register("pmih010x-2s-ibat-lvl1",NULL,&(struct thermal_zone_device_ops){.get_temp=unavailable_temperature},NULL);
 KUNIT_EXPECT_TRUE(test,IS_ERR(tz));if(IS_ERR(tz))KUNIT_EXPECT_EQ(test,PTR_ERR(tz),-EINVAL);else thermal_zone_device_unregister(tz);
}
static void of_voltage_trip_indices_program_expected_registers(struct kunit *test)
{
 struct fixture *f=test->priv;struct bcl_device *bcl;struct thermal_zone_device *tz;int i;
 const int temps[]={2800,2600,2300};const int expected[]={56,14,2};
 KUNIT_ASSERT_EQ(test,publish_nvmem(f),0);KUNIT_ASSERT_EQ(test,start_child(f),0);assert_bound(test,false);
 bcl=platform_get_drvdata(f->child);KUNIT_ASSERT_NOT_NULL(test,bcl);tz=bcl->param[2].tz_dev;KUNIT_ASSERT_NOT_NULL(test,tz);KUNIT_ASSERT_EQ(test,tz->num_trips,3);
 for(i=0;i<3;i++){
  KUNIT_EXPECT_EQ(test,tz->trips[i].trip.temperature,temps[i]);
  KUNIT_EXPECT_EQ(test,bcl_write_vbat_tz(tz,&tz->trips[i].trip,temps[i]),0);
  KUNIT_EXPECT_EQ(test,f->ram[0x4748+i],expected[i]);
 }
}
static struct kunit_case cases[]={KUNIT_CASE(native_probe_standard_pair),KUNIT_CASE(native_probe_2s_pair),KUNIT_CASE(late_nvmem_supplier_bind_retries),KUNIT_CASE(first_nvmem_read_error_propagates),KUNIT_CASE(second_nvmem_read_unwinds_voltage_zone),KUNIT_CASE(revision_failures_leave_no_resources),KUNIT_CASE(zone_failures_unwind_owned_irqs),KUNIT_CASE(monitor_enable_error_unwinds_all_zones),KUNIT_CASE(bpm_error_unwinds_registered_notifier),KUNIT_CASE(native_release_and_rebind),KUNIT_CASE(native_irq_thread_and_level_rearm),KUNIT_CASE(missing_parent_regmap_defers),KUNIT_CASE(original_2s_names_exceed_native_limit),KUNIT_CASE(of_voltage_trip_indices_program_expected_registers),{}};
static struct kunit_suite suite={.name="avr-bcl-pmic-probe",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
