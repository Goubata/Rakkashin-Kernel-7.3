// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Native 7.3 resource lifetime, table validation and acknowledged vote port.
 */
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/interconnect.h>
#include <linux/of.h>
#include <linux/overflow.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#define DDR_CDEV_NAME "ddr-cdev"
struct ddr_cdev {
 struct mutex lock;
 struct icc_path *icc_path;
 struct thermal_cooling_device *cdev;
 u32 *freq_table;
 unsigned int max_state,cur_state,desired_state;
 struct delayed_work retry;
 bool confirmed,stopping;
};
/* lock is held; only successful native ICC requests confirm a state. */
static int ddr_apply(struct ddr_cdev *d)
{
 int ret;
 if(d->confirmed && d->desired_state==d->cur_state)return 0;
 ret=icc_set_bw(d->icc_path,0,d->freq_table[d->desired_state]);
 d->confirmed=!ret;
 if(!ret)d->cur_state=d->desired_state;
 else mod_delayed_work(system_dfl_wq,&d->retry,msecs_to_jiffies(1000));
 return ret;
}
static void ddr_retry(struct work_struct *work)
{
 struct ddr_cdev *d=container_of(to_delayed_work(work),struct ddr_cdev,retry);
 mutex_lock(&d->lock);if(!d->stopping)ddr_apply(d);mutex_unlock(&d->lock);
}
static int ddr_set_cur_state(struct thermal_cooling_device *cdev,unsigned long state)
{
 struct ddr_cdev *d=cdev->devdata;int ret;
 mutex_lock(&d->lock);
 if(d->stopping){ret=-ENODEV;goto out;}
 if(state>d->max_state){ret=-EINVAL;goto out;}
 d->desired_state=state;ret=ddr_apply(d);
 if(!ret)cancel_delayed_work(&d->retry);
out:
 mutex_unlock(&d->lock);return ret;
}
static int ddr_get_cur_state(struct thermal_cooling_device *cdev,unsigned long *state)
{
 struct ddr_cdev *d=cdev->devdata;int ret=0;
 mutex_lock(&d->lock);
 if(d->stopping)ret=-ENODEV;
 else if(!d->confirmed)ret=-EAGAIN;
 else *state=d->cur_state;
 mutex_unlock(&d->lock);return ret;
}
static int ddr_get_max_state(struct thermal_cooling_device *cdev,unsigned long *state)
{
 struct ddr_cdev *d=cdev->devdata;int ret=0;
 mutex_lock(&d->lock);
 if(d->stopping)ret=-ENODEV;else *state=d->max_state;
 mutex_unlock(&d->lock);return ret;
}
static const struct thermal_cooling_device_ops ddr_cdev_ops={
 .set_cur_state=ddr_set_cur_state,.get_cur_state=ddr_get_cur_state,.get_max_state=ddr_get_max_state,
};
static int ddr_read_table(struct device *dev,struct ddr_cdev *d)
{
 struct device_node *np=dev->of_node;u32 width,cells;int count,ret,i;
 ret=of_property_read_u32(np,"#cooling-cells",&cells);if(ret || cells!=2)return -EINVAL;
 ret=of_property_read_u32(np,"qcom,bus-width",&width);if(ret)return ret;if(!width)return -EINVAL;
 count=of_property_count_u32_elems(np,"qcom,freq-table");if(count<=0)return count<0 ? count : -EINVAL;
 d->freq_table=devm_kcalloc(dev,(size_t)count+1,sizeof(*d->freq_table),GFP_KERNEL);if(!d->freq_table)return -ENOMEM;
 ret=of_property_read_u32_array(np,"qcom,freq-table",d->freq_table+1,count);if(ret)return ret;
 for(i=1;i<=count;i++){
  u32 vote;
  if(!d->freq_table[i])return -EINVAL;
  if(check_mul_overflow(d->freq_table[i],width,&vote))return -EOVERFLOW;
  d->freq_table[i]=vote;
 }
 d->max_state=count;return 0;
}
/* Registered before the native devm thermal resource: it runs after thermal
 * unregister has drained callbacks, and before the private context is freed. */
static void ddr_release_path(void *data)
{
 struct ddr_cdev *d=data;struct icc_path *path;
 mutex_lock(&d->lock);d->stopping=true;path=d->icc_path;d->icc_path=NULL;mutex_unlock(&d->lock);
 cancel_delayed_work_sync(&d->retry);
 /* Native icc_put sends a zero vote and removes consumer requests. */
 icc_put(path);
}
static int ddr_cdev_probe(struct platform_device *pdev)
{
 struct device *dev=&pdev->dev;struct ddr_cdev *d;int ret;
 d=devm_kzalloc(dev,sizeof(*d),GFP_KERNEL);if(!d)return -ENOMEM;mutex_init(&d->lock);INIT_DELAYED_WORK(&d->retry,ddr_retry);
 ret=ddr_read_table(dev,d);if(ret)return ret;
 d->icc_path=of_icc_get(dev,NULL);if(IS_ERR(d->icc_path))return dev_err_probe(dev,PTR_ERR(d->icc_path),"DDR interconnect path unavailable\n");
 if(!d->icc_path)return -ENODEV;
 ret=devm_add_action_or_reset(dev,ddr_release_path,d);if(ret)return ret;
 ret=icc_set_bw(d->icc_path,0,0);if(ret)return ret;d->confirmed=true;
 d->cdev=devm_thermal_of_cooling_device_register(dev,0,DDR_CDEV_NAME,d,&ddr_cdev_ops);
 if(IS_ERR(d->cdev))return PTR_ERR(d->cdev);
 platform_set_drvdata(pdev,d);return 0;
}
static void ddr_cdev_remove(struct platform_device *pdev)
{
 struct ddr_cdev *d=platform_get_drvdata(pdev);
 if(d){mutex_lock(&d->lock);d->stopping=true;mutex_unlock(&d->lock);cancel_delayed_work_sync(&d->retry);}
 platform_set_drvdata(pdev,NULL);
}
static const struct of_device_id ddr_cdev_match[]={{.compatible="qcom,ddr-cooling-device"},{}};
MODULE_DEVICE_TABLE(of,ddr_cdev_match);
static struct platform_driver ddr_cdev_driver={.probe=ddr_cdev_probe,.remove=ddr_cdev_remove,.driver={.name="ddr_cdev",.of_match_table=ddr_cdev_match}};
module_platform_driver(ddr_cdev_driver);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Qualcomm DDR cooling bandwidth request native 7.3 draft");
