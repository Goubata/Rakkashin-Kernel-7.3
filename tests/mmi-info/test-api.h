/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
#include <linux/proc_fs.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/platform_device.h>
#include <linux/soc/qcom/smem.h>
#include "mmi_info.h"
int avr_info_start(void);
void avr_info_stop(void);
struct platform_device *avr_info_component(unsigned int index);
int avr_info_lookup(const char *key,char **value,const char *property,bool lines);
int avr_info_smem_alloc(unsigned int host,unsigned int item,size_t size);
void *avr_info_smem_get(unsigned int host,unsigned int item,size_t *size);
struct proc_dir_entry *avr_info_proc_create(const char *name,umode_t mode,struct proc_dir_entry *parent,const struct proc_ops *ops);
int avr_info_sysfs_group(struct kobject *kobj,const struct attribute_group *group);
struct platform_device *avr_info_register_device(const char *name,int id,const struct resource *res,unsigned int count);
