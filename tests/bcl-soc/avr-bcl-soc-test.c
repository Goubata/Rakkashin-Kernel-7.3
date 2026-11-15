// SPDX-License-Identifier: GPL-2.0-only
/* Full production source; native power-supply, thermal, notifier, work/devres
 * and QoS registries. Only OF zone selection, policy lookup and faults recorded.
 * Runs only in a private QEMU guest, never in the phone's running kernel. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
static struct thermal_zone_device *test_zone(struct device *, int, void *, const struct thermal_zone_device_ops *);
static struct cpufreq_policy *test_policy_get(unsigned int);
static void test_policy_put(struct cpufreq_policy *);
static int test_qos_add(struct freq_constraints *, struct freq_qos_request *, enum freq_qos_req_type, s32);
static int test_notifier(struct notifier_block *);
#undef module_platform_driver
#define module_platform_driver(driver)
#define devm_thermal_of_zone_register test_zone
#define cpufreq_cpu_get test_policy_get
#define cpufreq_cpu_put test_policy_put
#define freq_qos_add_request test_qos_add
#define power_supply_reg_notifier test_notifier
#include "../../ports/qcom-bcl-soc/bcl_soc.c"
#undef devm_thermal_of_zone_register
#undef cpufreq_cpu_get
#undef cpufreq_cpu_put
#undef freq_qos_add_request
#undef power_supply_reg_notifier
#include "native-qos-test.h"
struct fixture {
 struct platform_device *pdev;
 struct power_supply *battery, *usb;
 struct cpufreq_policy policies[2];
 int refs[2], missing_cpu, add_calls, fail_add, zone_error, notifier_error;
 int present, capacity, usb_type, present_error, capacity_error, usb_error;
 bool use_of;
 struct device_node *sensor_node;
 struct thermal_cooling_device *cdev[3];
 unsigned long cooling_state[3];
 void *group;
 struct bcl_device *bcl;
};
static struct fixture *backend;
static enum power_supply_property batt_props[]={POWER_SUPPLY_PROP_PRESENT,POWER_SUPPLY_PROP_CAPACITY};
static enum power_supply_property usb_props[]={POWER_SUPPLY_PROP_USB_TYPE};
static int recorded_property(struct power_supply *psy, enum power_supply_property prop, union power_supply_propval *val)
{
 struct fixture *f=power_supply_get_drvdata(psy);
 switch(prop){
 case POWER_SUPPLY_PROP_PRESENT: if(READ_ONCE(f->present_error))return READ_ONCE(f->present_error);val->intval=READ_ONCE(f->present);return 0;
 case POWER_SUPPLY_PROP_CAPACITY: if(READ_ONCE(f->capacity_error))return READ_ONCE(f->capacity_error);val->intval=READ_ONCE(f->capacity);return 0;
 case POWER_SUPPLY_PROP_USB_TYPE: if(READ_ONCE(f->usb_error))return READ_ONCE(f->usb_error);val->intval=READ_ONCE(f->usb_type);return 0;
 default:return -EINVAL;
 }
}
static const struct power_supply_desc battery_desc={.name="battery",.type=POWER_SUPPLY_TYPE_BATTERY,.properties=batt_props,.num_properties=ARRAY_SIZE(batt_props),.get_property=recorded_property};
static const struct power_supply_desc usb_desc={.name="usb",.type=POWER_SUPPLY_TYPE_USB,.usb_types=BIT(POWER_SUPPLY_USB_TYPE_SDP)|BIT(POWER_SUPPLY_USB_TYPE_DCP),.properties=usb_props,.num_properties=ARRAY_SIZE(usb_props),.get_property=recorded_property};
static struct cpufreq_policy *test_policy_get(unsigned int cpu)
{
 if(cpu==backend->missing_cpu)return NULL;
 backend->refs[cpu%2]++;return &backend->policies[cpu%2];
}
static void test_policy_put(struct cpufreq_policy *policy)
{
 int i=policy-backend->policies;
 if(i<0 || i>1){pr_err("AVR_BCL_POLICY_FAIL\n");return;}
 backend->refs[i]--;
}
static int test_qos_add(struct freq_constraints *q,struct freq_qos_request *req,enum freq_qos_req_type type,s32 value)
{
 if(++backend->add_calls==backend->fail_add)return -EIO;
 return freq_qos_add_request(q,req,type,value);
}
static int test_notifier(struct notifier_block *nb)
{
 return backend->notifier_error ?: power_supply_reg_notifier(nb);
}
static void drop_zone(void *data){thermal_zone_device_unregister(data);}
static struct thermal_zone_device *test_zone(struct device *dev,int id,void *data,const struct thermal_zone_device_ops *ops)
{
 struct thermal_trip trip={.temperature=95,.hysteresis=0,.type=THERMAL_TRIP_PASSIVE};
 struct thermal_zone_device *tz;int ret;
 if(backend->zone_error)return ERR_PTR(backend->zone_error);
 if(backend->use_of)return devm_thermal_of_zone_register(dev,id,data,ops);
 tz=thermal_zone_device_register_with_trips("avr-bcl-private",&trip,1,data,ops,NULL,0,0);
 if(IS_ERR(tz))return tz;
 ret=devm_add_action_or_reset(dev,drop_zone,tz);
 if(ret)return ERR_PTR(ret);
 ret=thermal_zone_device_enable(tz);
 return ret ? ERR_PTR(ret) : tz;
}
static int start(struct fixture *f)
{
 int ret;
 f->group=devres_open_group(&f->pdev->dev,NULL,GFP_KERNEL);
 if(!f->group)return -ENOMEM;
 ret=bcl_soc_probe(f->pdev);
 if(ret){devres_release_group(&f->pdev->dev,f->group);f->group=NULL;return ret;}
 devres_close_group(&f->pdev->dev,f->group);f->bcl=platform_get_drvdata(f->pdev);
 flush_work(&f->bcl->soc_eval_work);
 return 0;
}
static void stop(struct fixture *f)
{
 if(!f->group)return;
 bcl_soc_remove(f->pdev);devres_release_group(&f->pdev->dev,f->group);f->group=NULL;f->bcl=NULL;
}
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
 struct power_supply_config cfg={};int i;
 if(!f)return -ENOMEM;
 backend=f;test->priv=f;boot_frequency=INT_MAX;f->missing_cpu=-1;f->present=1;f->capacity=2;f->usb_type=POWER_SUPPLY_USB_TYPE_SDP;
 for(i=0;i<2;i++)local_freq_constraints_init(&f->policies[i].constraints);
 f->pdev=platform_device_register_simple("avr-bcl-private",PLATFORM_DEVID_AUTO,NULL,0);
 if(IS_ERR(f->pdev))return PTR_ERR(f->pdev);
 cfg.drv_data=f;cfg.no_wakeup_source=true;
 f->battery=power_supply_register(&f->pdev->dev,&battery_desc,&cfg);
 if(IS_ERR(f->battery)){i=PTR_ERR(f->battery);f->battery=NULL;goto fail;}
 flush_delayed_work(&f->battery->deferred_register_work);flush_work(&f->battery->changed_work);
 f->usb=power_supply_register(&f->pdev->dev,&usb_desc,&cfg);
 if(IS_ERR(f->usb)){i=PTR_ERR(f->usb);f->usb=NULL;power_supply_unregister(f->battery);f->battery=NULL;goto fail;}
 flush_delayed_work(&f->usb->deferred_register_work);flush_work(&f->usb->changed_work);
 return 0;
fail:platform_device_unregister(f->pdev);return i;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;stop(f);
 KUNIT_EXPECT_EQ(test,f->refs[0],0);KUNIT_EXPECT_EQ(test,f->refs[1],0);
 if(f->usb){KUNIT_EXPECT_EQ(test,atomic_read(&f->usb->use_cnt),1);power_supply_unregister(f->usb);}
 if(f->battery){KUNIT_EXPECT_EQ(test,atomic_read(&f->battery->use_cnt),1);power_supply_unregister(f->battery);}
 {int i;for(i=0;i<3;i++)if(f->cdev[i])thermal_cooling_device_unregister(f->cdev[i]);}
 of_node_put(f->sensor_node);f->pdev->dev.of_node=NULL;
 platform_device_unregister(f->pdev);backend=NULL;boot_frequency=INT_MAX;
}
static void all_capacity_values(struct kunit *test)
{
 struct fixture *f=test->priv;int i,value=-1;KUNIT_ASSERT_EQ(test,start(f),0);
 for(i=0;i<=100;i++){WRITE_ONCE(f->capacity,i);KUNIT_ASSERT_EQ(test,bcl_read_soc(f->bcl->tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,100-i);}
}
static void absent_battery(struct kunit *test)
{
 struct fixture *f=test->priv;int value=-1;KUNIT_ASSERT_EQ(test,start(f),0);WRITE_ONCE(f->present,0);WRITE_ONCE(f->capacity_error,-EIO);
 KUNIT_EXPECT_EQ(test,bcl_read_soc(f->bcl->tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,0);
}
static void invalid_properties(struct kunit *test)
{
 struct fixture *f=test->priv;int value=777;KUNIT_ASSERT_EQ(test,start(f),0);WRITE_ONCE(f->present,2);
 KUNIT_EXPECT_EQ(test,bcl_read_soc(f->bcl->tz_dev,&value),-ERANGE);KUNIT_EXPECT_EQ(test,value,777);
 WRITE_ONCE(f->present,1);WRITE_ONCE(f->capacity,-1);KUNIT_EXPECT_EQ(test,bcl_read_soc(f->bcl->tz_dev,&value),-ERANGE);
 WRITE_ONCE(f->capacity,101);KUNIT_EXPECT_EQ(test,bcl_read_soc(f->bcl->tz_dev,&value),-ERANGE);KUNIT_EXPECT_EQ(test,value,777);
}
static void property_errors_preserve_output(struct kunit *test)
{
 struct fixture *f=test->priv;int value=777;KUNIT_ASSERT_EQ(test,start(f),0);WRITE_ONCE(f->present_error,-EIO);
 KUNIT_EXPECT_EQ(test,bcl_read_soc(f->bcl->tz_dev,&value),-EIO);KUNIT_EXPECT_EQ(test,value,777);
 WRITE_ONCE(f->present_error,0);WRITE_ONCE(f->capacity_error,-EAGAIN);KUNIT_EXPECT_EQ(test,bcl_read_soc(f->bcl->tz_dev,&value),-EAGAIN);KUNIT_EXPECT_EQ(test,value,777);
 WRITE_ONCE(f->usb_error,-ENODATA);KUNIT_EXPECT_EQ(test,soc_read_usb_charger_type(f->bcl,&value),-ENODATA);KUNIT_EXPECT_EQ(test,value,777);
}
static void missing_battery_defers(struct kunit *test)
{
 struct fixture *f=test->priv;power_supply_unregister(f->battery);f->battery=NULL;KUNIT_EXPECT_EQ(test,start(f),-EPROBE_DEFER);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev),NULL);
}
static void missing_usb_unwinds_battery(struct kunit *test)
{
 struct fixture *f=test->priv;power_supply_unregister(f->usb);f->usb=NULL;KUNIT_EXPECT_EQ(test,start(f),-EPROBE_DEFER);KUNIT_EXPECT_EQ(test,atomic_read(&f->battery->use_cnt),1);
}
static void zone_failure_unwinds(struct kunit *test)
{
 struct fixture *f=test->priv;f->zone_error=-ENODEV;KUNIT_EXPECT_EQ(test,start(f),-ENODEV);KUNIT_EXPECT_EQ(test,atomic_read(&f->battery->use_cnt),1);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev),NULL);
}
static void notifier_failure_unwinds(struct kunit *test)
{
 struct fixture *f=test->priv;f->notifier_error=-EIO;KUNIT_EXPECT_EQ(test,start(f),-EIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->usb->use_cnt),1);KUNIT_EXPECT_PTR_EQ(test,platform_get_drvdata(f->pdev),NULL);
}
static void thresholds_and_native_cache(struct kunit *test)
{
 struct fixture *f=test->priv;int value;KUNIT_ASSERT_EQ(test,start(f),0);
 KUNIT_EXPECT_EQ(test,bcl_set_soc(f->bcl->tz_dev,0,-1),-ERANGE);KUNIT_EXPECT_EQ(test,bcl_set_soc(f->bcl->tz_dev,0,101),-ERANGE);
 KUNIT_ASSERT_EQ(test,bcl_set_soc(f->bcl->tz_dev,0,95),0);flush_work(&f->bcl->soc_eval_work);
 KUNIT_ASSERT_EQ(test,thermal_zone_get_temp(f->bcl->tz_dev,&value),0);KUNIT_EXPECT_EQ(test,value,98);
 KUNIT_EXPECT_EQ(test,READ_ONCE(f->bcl->tz_dev->temperature),98);
 KUNIT_ASSERT_EQ(test,bcl_set_soc(f->bcl->tz_dev,0,INT_MAX),0);KUNIT_EXPECT_FALSE(test,f->bcl->irq_enabled);
}
static void trend_hysteresis(struct kunit *test)
{
 struct fixture *f=test->priv;enum thermal_trend trend;struct thermal_trip trip={.temperature=95,.hysteresis=2};KUNIT_ASSERT_EQ(test,start(f),0);
 WRITE_ONCE(f->bcl->tz_dev->temperature,95);KUNIT_ASSERT_EQ(test,bcl_soc_get_trend(f->bcl->tz_dev,&trip,&trend),0);KUNIT_EXPECT_EQ(test,trend,THERMAL_TREND_RAISING);
 WRITE_ONCE(f->bcl->tz_dev->temperature,94);bcl_soc_get_trend(f->bcl->tz_dev,&trip,&trend);KUNIT_EXPECT_EQ(test,trend,THERMAL_TREND_STABLE);
 WRITE_ONCE(f->bcl->tz_dev->temperature,93);bcl_soc_get_trend(f->bcl->tz_dev,&trip,&trend);KUNIT_EXPECT_EQ(test,trend,THERMAL_TREND_DROPPING);
}
static void real_notifier_and_remove(struct kunit *test)
{
 struct fixture *f=test->priv;struct bcl_device *bcl;WRITE_ONCE(f->capacity,20);KUNIT_ASSERT_EQ(test,start(f),0);bcl=f->bcl;
 KUNIT_EXPECT_EQ(test,atomic_read(&f->battery->use_cnt),2);KUNIT_EXPECT_EQ(test,atomic_read(&f->usb->use_cnt),2);
 WRITE_ONCE(f->capacity,1);power_supply_changed(f->battery);flush_work(&f->battery->changed_work);flush_work(&bcl->soc_eval_work);
 KUNIT_EXPECT_EQ(test,READ_ONCE(bcl->tz_dev->temperature),99);
 bcl_soc_remove(f->pdev);KUNIT_EXPECT_FALSE(test,bcl->notifier_registered);KUNIT_EXPECT_FALSE(test,work_pending(&bcl->soc_eval_work));KUNIT_EXPECT_FALSE(test,delayed_work_pending(&bcl->boot_work));
 KUNIT_EXPECT_EQ(test,battery_supply_callback(&bcl->psy_nb,PSY_EVENT_PROP_CHANGED,f->battery),NOTIFY_OK);KUNIT_EXPECT_FALSE(test,work_pending(&bcl->soc_eval_work));
 KUNIT_EXPECT_EQ(test,bcl_set_soc(bcl->tz_dev,0,90),-ENODEV);KUNIT_EXPECT_EQ(test,bcl_set_soc(bcl->tz_dev,0,INT_MAX),0);bcl_soc_remove(f->pdev);stop(f);
}
static void wrong_notifier_event(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_EQ(test,start(f),0);
 KUNIT_EXPECT_EQ(test,battery_supply_callback(&f->bcl->psy_nb,PSY_EVENT_PROP_CHANGED+1,f->battery),NOTIFY_DONE);
 KUNIT_EXPECT_EQ(test,battery_supply_callback(&f->bcl->psy_nb,PSY_EVENT_PROP_CHANGED,NULL),NOTIFY_DONE);
}
static void repeated_probe_remove(struct kunit *test)
{
 struct fixture *f=test->priv;int i;for(i=0;i<4;i++){KUNIT_ASSERT_EQ(test,start(f),0);stop(f);KUNIT_EXPECT_EQ(test,atomic_read(&f->battery->use_cnt),1);}
}
static void qos_shared_policies(struct kunit *test)
{
 struct fixture *f=test->priv;struct freq_qos_request other={};int i;
 KUNIT_ASSERT_GE(test,num_possible_cpus(),2);boot_frequency=500000;
 KUNIT_ASSERT_GE(test,freq_qos_add_request(&f->policies[0].constraints,&other,FREQ_QOS_MAX,900000),0);
 KUNIT_ASSERT_EQ(test,start(f),0);cancel_delayed_work_sync(&f->bcl->boot_work);
 KUNIT_EXPECT_EQ(test,f->add_calls,2);for(i=0;i<2;i++){KUNIT_EXPECT_EQ(test,f->refs[i],1);KUNIT_EXPECT_EQ(test,local_freq_qos_read_value(&f->policies[i].constraints,FREQ_QOS_MAX),500000);}
 stop(f);KUNIT_EXPECT_TRUE(test,freq_qos_request_active(&other));KUNIT_EXPECT_EQ(test,local_freq_qos_read_value(&f->policies[0].constraints,FREQ_QOS_MAX),900000);freq_qos_remove_request(&other);
}
static void qos_add_failure_rolls_back(struct kunit *test)
{
 struct fixture *f=test->priv;boot_frequency=500000;f->fail_add=2;KUNIT_EXPECT_EQ(test,start(f),-EIO);KUNIT_EXPECT_EQ(test,f->refs[0],0);KUNIT_EXPECT_EQ(test,f->refs[1],0);KUNIT_EXPECT_EQ(test,local_freq_qos_read_value(&f->policies[0].constraints,FREQ_QOS_MAX),FREQ_QOS_MAX_DEFAULT_VALUE);
}
static void missing_policy_rolls_back(struct kunit *test)
{
 struct fixture *f=test->priv;boot_frequency=500000;f->missing_cpu=1;KUNIT_EXPECT_EQ(test,start(f),-EPROBE_DEFER);KUNIT_EXPECT_EQ(test,f->refs[0],0);
}
static void boot_cap_retained_on_unknown(struct kunit *test)
{
 struct fixture *f=test->priv;boot_frequency=500000;KUNIT_ASSERT_EQ(test,start(f),0);cancel_delayed_work_sync(&f->bcl->boot_work);
 WRITE_ONCE(f->capacity_error,-EIO);bcl_boot_work(&f->bcl->boot_work.work);cancel_delayed_work_sync(&f->bcl->boot_work);KUNIT_EXPECT_TRUE(test,f->bcl->boot_limit_enabled);
 WRITE_ONCE(f->capacity_error,0);WRITE_ONCE(f->usb_error,-EIO);bcl_boot_work(&f->bcl->boot_work.work);cancel_delayed_work_sync(&f->bcl->boot_work);KUNIT_EXPECT_TRUE(test,f->bcl->boot_limit_enabled);
 WRITE_ONCE(f->usb_error,0);bcl_boot_work(&f->bcl->boot_work.work);cancel_delayed_work_sync(&f->bcl->boot_work);KUNIT_EXPECT_TRUE(test,f->bcl->boot_limit_enabled);
}
static void boot_cap_clears_on_known_recovery(struct kunit *test)
{
 struct fixture *f=test->priv;boot_frequency=500000;KUNIT_ASSERT_EQ(test,start(f),0);cancel_delayed_work_sync(&f->bcl->boot_work);
 WRITE_ONCE(f->capacity,4);bcl_boot_work(&f->bcl->boot_work.work);KUNIT_EXPECT_FALSE(test,f->bcl->boot_limit_enabled);KUNIT_EXPECT_EQ(test,f->refs[0],0);stop(f);
 WRITE_ONCE(f->capacity,2);KUNIT_ASSERT_EQ(test,start(f),0);cancel_delayed_work_sync(&f->bcl->boot_work);
 WRITE_ONCE(f->usb_type,POWER_SUPPLY_USB_TYPE_DCP);bcl_boot_work(&f->bcl->boot_work.work);KUNIT_EXPECT_FALSE(test,f->bcl->boot_limit_enabled);
}
static int cooling_max(struct thermal_cooling_device *cdev,unsigned long *state){*state=4;return 0;}
static int cooling_get(struct thermal_cooling_device *cdev,unsigned long *state){*state=READ_ONCE(*(unsigned long *)cdev->devdata);return 0;}
static int cooling_set(struct thermal_cooling_device *cdev,unsigned long state){WRITE_ONCE(*(unsigned long *)cdev->devdata,state);return 0;}
static const struct thermal_cooling_device_ops cooling_ops={.get_max_state=cooling_max,.get_cur_state=cooling_get,.set_cur_state=cooling_set};
static void attach_of(struct kunit *test,struct fixture *f)
{
 f->sensor_node=of_find_node_by_path("/avr-bcl-private");KUNIT_ASSERT_NOT_NULL(test,f->sensor_node);
 f->pdev->dev.of_node=f->sensor_node;f->use_of=true;
}
static void native_of_depletion_and_cooling_links(struct kunit *test)
{
 struct fixture *f=test->priv;int i,value,low=-1,high=-1;struct thermal_zone_device *tz;
 attach_of(test,f);KUNIT_ASSERT_NOT_NULL(test,f->sensor_node);WRITE_ONCE(f->capacity,20);
 for(i=0;i<3;i++){
  char path[32];struct device_node *np;
  snprintf(path,sizeof(path),"/avr-cooling%d",i);np=of_find_node_by_path(path);KUNIT_ASSERT_NOT_NULL(test,np);
  f->cdev[i]=thermal_of_cooling_device_register(np,0,"avr-recorded-cooling",&f->cooling_state[i],&cooling_ops);of_node_put(np);
  if(IS_ERR(f->cdev[i])){long err=PTR_ERR(f->cdev[i]);f->cdev[i]=NULL;KUNIT_FAIL(test,"cooling registration %ld",err);return;}
 }
 KUNIT_ASSERT_EQ(test,start(f),0);tz=f->bcl->tz_dev;
 KUNIT_EXPECT_STREQ(test,thermal_zone_device_type(tz),"avr-socd");KUNIT_ASSERT_EQ(test,tz->num_trips,2);
 for(i=0;i<tz->num_trips;i++){
  if(tz->trips[i].trip.temperature==90)low=i;
  if(tz->trips[i].trip.temperature==95)high=i;
  KUNIT_EXPECT_EQ(test,tz->trips[i].trip.hysteresis,0);KUNIT_EXPECT_EQ(test,tz->trips[i].trip.type,THERMAL_TRIP_PASSIVE);
 }
 KUNIT_ASSERT_GE(test,low,0);KUNIT_ASSERT_GE(test,high,0);KUNIT_EXPECT_NE(test,low,high);
 KUNIT_EXPECT_EQ(test,tz->passive_delay_jiffies,msecs_to_jiffies(100));KUNIT_EXPECT_EQ(test,tz->polling_delay_jiffies,0ul);
 for(i=0;i<3;i++){
  KUNIT_EXPECT_FALSE(test,thermal_trip_is_bound_to_cdev(tz,&tz->trips[low].trip,f->cdev[i]));
  KUNIT_EXPECT_TRUE(test,thermal_trip_is_bound_to_cdev(tz,&tz->trips[high].trip,f->cdev[i]));
 }
 mutex_lock(&tz->lock);
 for(i=0;i<3;i++){
  struct thermal_instance *instance;int count=0;
  list_for_each_entry(instance,&tz->trips[high].thermal_instances,trip_node){
   if(instance->cdev!=f->cdev[i])continue;
   count++;KUNIT_EXPECT_EQ(test,instance->lower,0ul);KUNIT_EXPECT_EQ(test,instance->upper,i ? 4ul : 1ul);
  }
  KUNIT_EXPECT_EQ(test,count,1);
 }
 mutex_unlock(&tz->lock);
 WRITE_ONCE(f->capacity,1);power_supply_changed(f->battery);flush_work(&f->battery->changed_work);flush_work(&f->bcl->soc_eval_work);
 KUNIT_ASSERT_EQ(test,thermal_zone_get_temp(tz,&value),0);KUNIT_EXPECT_EQ(test,value,99);KUNIT_EXPECT_EQ(test,READ_ONCE(tz->temperature),99);
 stop(f);KUNIT_EXPECT_EQ(test,atomic_read(&f->battery->use_cnt),1);
}
static void native_of_repeated_zone_lifetime(struct kunit *test)
{
 struct fixture *f=test->priv;int i;attach_of(test,f);KUNIT_ASSERT_NOT_NULL(test,f->sensor_node);
 for(i=0;i<3;i++){KUNIT_ASSERT_EQ(test,start(f),0);KUNIT_EXPECT_STREQ(test,thermal_zone_device_type(f->bcl->tz_dev),"avr-socd");stop(f);}
}
static struct kunit_case cases[]={
 KUNIT_CASE(all_capacity_values),KUNIT_CASE(absent_battery),KUNIT_CASE(invalid_properties),KUNIT_CASE(property_errors_preserve_output),
 KUNIT_CASE(missing_battery_defers),KUNIT_CASE(missing_usb_unwinds_battery),KUNIT_CASE(zone_failure_unwinds),KUNIT_CASE(notifier_failure_unwinds),
 KUNIT_CASE(thresholds_and_native_cache),KUNIT_CASE(trend_hysteresis),KUNIT_CASE(real_notifier_and_remove),KUNIT_CASE(wrong_notifier_event),
 KUNIT_CASE(repeated_probe_remove),KUNIT_CASE(qos_shared_policies),KUNIT_CASE(qos_add_failure_rolls_back),KUNIT_CASE(missing_policy_rolls_back),
 KUNIT_CASE(boot_cap_retained_on_unknown),KUNIT_CASE(boot_cap_clears_on_known_recovery),KUNIT_CASE(native_of_depletion_and_cooling_links),KUNIT_CASE(native_of_repeated_zone_lifetime),{}
};
static struct kunit_suite suite={.name="avr-bcl-soc",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_DESCRIPTION("Private native BCL SoC framework and recorded-policy tests");
