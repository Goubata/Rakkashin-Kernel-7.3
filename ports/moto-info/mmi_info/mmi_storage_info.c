/*
 * Copyright (C) 2016 Motorola Mobility LLC
 * Copyright (C) 2018 Motorola Mobility LLC
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/slab.h>
#include <linux/mmi_annotate.h>
#include "mmi_info.h"

static struct mmi_storage_info *info;

SYSFS_SIMPLE_SHOW(fw, info->firmware_version, "%s", 32)
SYSFS_SIMPLE_SHOW(model, info->product_name, "%s", 32)
SYSFS_SIMPLE_SHOW(vendor, info->card_manufacturer, "%s", 32)
SYSFS_SIMPLE_SHOW(size, info->size, "%s", 16)
SYSFS_SIMPLE_SHOW(type, info->type, "%s", 16)

SYSFS_ATTRIBUTE(fw, 0444)
SYSFS_ATTRIBUTE(model, 0444)
SYSFS_ATTRIBUTE(vendor, 0444)
SYSFS_ATTRIBUTE(size, 0444)
SYSFS_ATTRIBUTE(type, 0444)

static struct attribute *st_info_properties_attrs[] = {
	&fw_attr.attr,
	&model_attr.attr,
	&vendor_attr.attr,
	&size_attr.attr,
	&type_attr.attr,
	NULL
};

static struct attribute_group st_info_properties_attr_group = {
	.attrs = st_info_properties_attrs,
};

static struct kobject *st_info_properties_kobj;

static int mmi_storage_field(struct device_node *node, const char *name,
			     char *destination, size_t size)
{
	const char *value;
	int ret;

	if (!of_find_property(node, name, NULL))
		return 0;
	ret = of_property_read_string(node, name, &value);
	if (ret)
		return ret;
	strscpy(destination, value, size);
	return 0;
}

int mmi_storage_info_init(void)
{
	struct device_node *node;
	int ret;

	node = of_find_node_by_path("/chosen/mmi,storage");
	if (!node)
		return -ENODEV;
	info = kzalloc(sizeof(*info), GFP_KERNEL);
	if (!info) {
		of_node_put(node);
		return -ENOMEM;
	}
#define READ_STORAGE(field, property) do { \
	ret = mmi_storage_field(node, property, info->field, sizeof(info->field)); \
	if (ret) goto put_node; \
} while (0)
	READ_STORAGE(type, "type");
	READ_STORAGE(size, "size");
	READ_STORAGE(card_manufacturer, "manufacturer");
	READ_STORAGE(product_name, "product");
	READ_STORAGE(firmware_version, "firmware");
#undef READ_STORAGE
put_node:
	of_node_put(node);
	if (ret)
		goto free_info;
	st_info_properties_kobj = kobject_create_and_add("storage", NULL);
	if (!st_info_properties_kobj) {
		ret = -ENOMEM;
		goto free_info;
	}
	ret = sysfs_create_group(st_info_properties_kobj, &st_info_properties_attr_group);
	if (ret) {
		kobject_put(st_info_properties_kobj);
		st_info_properties_kobj = NULL;
		goto free_info;
	}
	mmi_annotate_persist("%s: %s %s %s FV=%s\n", info->type, info->size,
			     info->card_manufacturer, info->product_name, info->firmware_version);
	return 0;
free_info:
	kfree(info);
	info = NULL;
	return ret;
}

void mmi_storage_info_exit(void)
{
	if (st_info_properties_kobj) {
		sysfs_remove_group(st_info_properties_kobj, &st_info_properties_attr_group);
		kobject_put(st_info_properties_kobj);
		st_info_properties_kobj = NULL;
	}
	kfree(info);
	info = NULL;
}
