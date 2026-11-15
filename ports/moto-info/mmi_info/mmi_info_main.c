/*
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
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/list.h>
#include <linux/kernel.h>
#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/mmi_device.h>
#include "mmi_info.h"

struct mmi_chosen_info mmi_chosen_data;

/* Immutable parsed values remain valid until the provider is stopped. Internal
 * callers copy them during probe; sysfs/proc readers use the owned snapshots. */
struct mmi_bootarg_pair {
	struct list_head link;
	char *name, *value;
};
struct mmi_bootarg_property {
	struct list_head link, pairs;
	char *name, *text;
	bool lines;
};
static LIST_HEAD(bootarg_properties);
static DEFINE_MUTEX(bootarg_lock);
static struct platform_device *mmi_info_devices[4];

bool mmi_device_is_available(struct device_node *np)
{
	struct device_node *target;
	const char *path, *property, *selected, *value;
	bool contains = false, found = false;
	int count, i;

	if (!np || !of_find_property(np, "mmi,status", NULL))
		return true;
	count = of_property_count_strings(np, "mmi,status");
	if (count < 3 || of_property_read_string_index(np, "mmi,status", 0, &path) ||
	    of_property_read_string_index(np, "mmi,status", 1, &property) ||
	    !*path || !*property)
		return false;
	target = of_find_node_by_path(path);
	if (!target)
		return false;
	if (of_property_read_string(target, property, &selected))
		goto out;
	for (i = 2; i < count; i++) {
		if (of_property_read_string_index(np, "mmi,status", i, &value))
			break;
		if (i == 2 && !strcmp(value, "contains")) {
			contains = true;
			continue;
		}
		if ((!contains && !strcmp(selected, value)) ||
		    (contains && strstr(selected, value))) {
			found = true;
			break;
		}
	}
out:
	of_node_put(target);
	return found;
}
EXPORT_SYMBOL(mmi_device_is_available);

bool mmi_check_dynamic_device_node(char *dev_name)
{
	struct device_node *chosen, *target;
	const char *path;
	bool result = true;
	int count, i;

	if (!dev_name || !*dev_name)
		return false;
	chosen = of_find_node_by_path("/chosen");
	if (!chosen)
		return true;
	if (!of_find_property(chosen, "mmi,dynamic_devices", NULL))
		goto out;
	count = of_property_count_strings(chosen, "mmi,dynamic_devices");
	if (count < 0) {
		result = false;
		goto out;
	}
	for (i = 0; i < count; i++) {
		if (of_property_read_string_index(chosen, "mmi,dynamic_devices", i, &path)) {
			result = false;
			break;
		}
		if (!strstr(path, dev_name))
			continue;
		target = of_find_node_by_path(path);
		if (target) {
			result = mmi_device_is_available(target);
			of_node_put(target);
		}
		break;
	}
out:
	of_node_put(chosen);
	return result;
}
EXPORT_SYMBOL(mmi_check_dynamic_device_node);

static void mmi_bootarg_property_free(struct mmi_bootarg_property *property)
{
	struct mmi_bootarg_pair *pair, *next;

	list_for_each_entry_safe(pair, next, &property->pairs, link) {
		list_del(&pair->link);
		kfree(pair);
	}
	kfree(property->text);
	kfree(property->name);
	kfree(property);
}

static int mmi_bootarg_add(struct mmi_bootarg_property *property, char *name, char *value)
{
	struct mmi_bootarg_pair *pair;

	if (!value || !*name)
		return 0;
	pair = kzalloc(sizeof(*pair), GFP_KERNEL);
	if (!pair)
		return -ENOMEM;
	pair->name = name;
	pair->value = value;
	list_add_tail(&pair->link, &property->pairs);
	return 0;
}

static int mmi_bootarg_parse(struct mmi_bootarg_property *property)
{
	char *cursor = property->text, *name, *value, *line;
	int ret;
	size_t length;

	if (!property->lines) {
		cursor = skip_spaces(cursor);
		while (*cursor) {
			cursor = next_arg(cursor, &name, &value);
			ret = mmi_bootarg_add(property, name, value);
			if (ret)
				return ret;
		}
		return 0;
	}
	while ((line = strsep(&cursor, "\n"))) {
		line = strim(line);
		if (!*line || *line == '#')
			continue;
		value = strchr(line, '=');
		if (!value)
			continue;
		*value++ = 0;
		name = strim(line);
		value = strim(value);
		length = strlen(value);
		if (length && *value == '"') {
			if (length < 2 || value[length - 1] != '"')
				return -EINVAL;
			value[length - 1] = 0;
			value++;
		}
		ret = mmi_bootarg_add(property, name, value);
		if (ret)
			return ret;
	}
	return 0;
}

static int mmi_get_bootarg_dt(const char *key, char **value,
			    const char *name, const char *separator)
{
	struct mmi_bootarg_property *property, *cached = NULL;
	struct mmi_bootarg_pair *pair;
	struct device_node *chosen;
	const char *raw;
	size_t key_length;
	bool lines;
	int ret;

	if (!value)
		return -EINVAL;
	*value = NULL;
	if (!key || !name || !separator ||
	    (strcmp(separator, " ") && strcmp(separator, "\n")))
		return -EINVAL;
	key_length = strlen(key);
	if (key_length < 2 || strchr(key, '=') != key + key_length - 1)
		return -EINVAL;
	lines = !strcmp(separator, "\n");
	mutex_lock(&bootarg_lock);
	list_for_each_entry(property, &bootarg_properties, link) {
		if (property->lines == lines && !strcmp(property->name, name)) {
			cached = property;
			break;
		}
	}
	if (!cached) {
		chosen = of_find_node_by_path("/chosen");
		if (!chosen) {
			ret = -ENOENT;
			goto out;
		}
		ret = of_property_read_string(chosen, name, &raw);
		if (ret) {
			if (!of_find_property(chosen, name, NULL))
				ret = -ENOENT;
			of_node_put(chosen);
			goto out;
		}
		property = kzalloc(sizeof(*property), GFP_KERNEL);
		if (!property) {
			of_node_put(chosen);
			ret = -ENOMEM;
			goto out;
		}
		INIT_LIST_HEAD(&property->pairs);
		property->name = kstrdup(name, GFP_KERNEL);
		property->text = kstrdup(raw, GFP_KERNEL);
		property->lines = lines;
		of_node_put(chosen);
		if (!property->name || !property->text)
			ret = -ENOMEM;
		else
			ret = mmi_bootarg_parse(property);
		if (ret) {
			mmi_bootarg_property_free(property);
			goto out;
		}
		list_add_tail(&property->link, &bootarg_properties);
		cached = property;
	}
	ret = -ENOENT;
	list_for_each_entry(pair, &cached->pairs, link) {
		if (strlen(pair->name) == key_length - 1 &&
		    !memcmp(pair->name, key, key_length - 1)) {
			*value = pair->value;
			ret = 0;
			break;
		}
	}
out:
	mutex_unlock(&bootarg_lock);
	return ret;
}

int mmi_get_bootarg(const char *key, char **value)
{
#ifdef CONFIG_BOOT_CONFIG
	return mmi_get_bootarg_dt(key, value, "mmi,bootconfig", "\n");
#else
	return mmi_get_bootarg_dt(key, value, "bootargs", " ");
#endif
}

int mmi_copy_bootarg(const char *key, char *destination, size_t size)
{
	char *value;
	int ret = mmi_get_bootarg(key, &value);

	if (ret == -ENOENT)
		return 0;
	if (ret)
		return ret;
	strscpy(destination, value, size);
	return 0;
}

static void mmi_bootargs_cleanup(void)
{
	struct mmi_bootarg_property *property, *next;

	mutex_lock(&bootarg_lock);
	list_for_each_entry_safe(property, next, &bootarg_properties, link) {
		list_del(&property->link);
		mmi_bootarg_property_free(property);
	}
	mutex_unlock(&bootarg_lock);
}

static void mmi_of_populate_setup(void)
{
	const char *temp;
	struct device_node *n = of_find_node_by_path("/chosen");

	if (n == NULL)
		return;

	of_property_read_u32(n, "mmi,powerup_reason",
		&mmi_chosen_data.powerup_reason);
	of_property_read_u32(n, "mmi,mbmversion", &mmi_chosen_data.mbmversion);
	of_property_read_u32(n, "mmi,boot_seq", &mmi_chosen_data.boot_seq);
	of_property_read_u32(n, "mmi,prod_id", &mmi_chosen_data.product_id);
	of_property_read_u32(n, "linux,hwrev", &mmi_chosen_data.system_rev);
	of_property_read_u32(n, "linux,seriallow",
		&mmi_chosen_data.system_serial_low);
	of_property_read_u32(n, "linux,serialhigh",
		&mmi_chosen_data.system_serial_high);
	if (of_property_read_string(n, "mmi,baseband", &temp) == 0)
		strscpy(mmi_chosen_data.baseband, temp, BASEBAND_MAX_LEN);
	if (of_property_read_string(n, "mmi,msm_hw", &temp) == 0)
		strscpy(mmi_chosen_data.msm_hw, temp, MSMHW_MAX_LEN);
	if (of_property_read_string(n, "mmi,chipid", &temp) == 0)
		strscpy(mmi_chosen_data.chipid, temp, CHIPID_MAX_LEN);

	of_node_put(n);
}

struct mmi_info_component {
	int (*init)(void);
	void (*exit)(void);
};
static const struct mmi_info_component mmi_info_components[] = {
	{ mmi_storage_info_init, mmi_storage_info_exit },
	{ mmi_ram_info_init, mmi_ram_info_exit },
	{ mmi_unit_info_init, mmi_unit_info_exit },
	{ mmi_boot_info_init, mmi_boot_info_exit },
};
static const struct platform_device_id mmi_info_ids[] = {
	{ "mmi_info_storage", 0 }, { "mmi_info_ram", 1 },
	{ "mmi_info_unit", 2 }, { "mmi_info_boot", 3 }, { }
};
MODULE_DEVICE_TABLE(platform, mmi_info_ids);

static int mmi_info_probe(struct platform_device *pdev)
{
	const struct platform_device_id *id = platform_get_device_id(pdev);
	const struct mmi_info_component *component;
	int ret;

	if (!id || id->driver_data >= ARRAY_SIZE(mmi_info_components))
		return -EINVAL;
	component = &mmi_info_components[id->driver_data];
	ret = component->init();
	if (ret)
		return dev_err_probe(&pdev->dev, ret, "metadata component not ready\n");
	platform_set_drvdata(pdev, (void *)component);
	return 0;
}

static void mmi_info_remove(struct platform_device *pdev)
{
	const struct mmi_info_component *component = platform_get_drvdata(pdev);

	component->exit();
	platform_set_drvdata(pdev, NULL);
}

/* Software metadata endpoints have independent probe lifetimes. A deferred
 * RAM or unit-info SMEM request must not block boot info or DT selectors. */
static struct platform_driver mmi_info_driver = {
	.probe = mmi_info_probe,
	.remove = mmi_info_remove,
	.id_table = mmi_info_ids,
	.driver = { .name = "mmi_info" },
};

static int mmi_hw_info_start(void)
{
	int ret, i;

	memset(&mmi_chosen_data, 0, sizeof(mmi_chosen_data));
	mmi_of_populate_setup();
	/* Public boot-mode getters are ready before this module becomes visible. */
	ret = mmi_boot_info_prepare();
	if (ret)
		goto cleanup;
	ret = platform_driver_register(&mmi_info_driver);
	if (ret)
		goto cleanup;
	for (i = 0; i < ARRAY_SIZE(mmi_info_components); i++) {
		mmi_info_devices[i] = platform_device_register_simple(mmi_info_ids[i].name,
							    PLATFORM_DEVID_AUTO, NULL, 0);
		if (IS_ERR(mmi_info_devices[i])) {
			ret = PTR_ERR(mmi_info_devices[i]);
			mmi_info_devices[i] = NULL;
			while (i--)
				platform_device_unregister(mmi_info_devices[i]);
			memset(mmi_info_devices, 0, sizeof(mmi_info_devices));
			platform_driver_unregister(&mmi_info_driver);
			goto cleanup;
		}
	}
	return 0;
cleanup:
	mmi_bootargs_cleanup();
	return ret;
}

static int __init mmi_hw_info_init(void)
{
	return mmi_hw_info_start();
}

static void mmi_hw_info_exit(void)
{
	int i;

	for (i = ARRAY_SIZE(mmi_info_devices) - 1; i >= 0; i--) {
		if (mmi_info_devices[i]) {
			platform_device_unregister(mmi_info_devices[i]);
			mmi_info_devices[i] = NULL;
		}
	}
	platform_driver_unregister(&mmi_info_driver);
	mmi_bootargs_cleanup();
}

module_init(mmi_hw_info_init);
module_exit(mmi_hw_info_exit);
MODULE_DESCRIPTION("Motorola Mobility LLC. HW Info");
MODULE_LICENSE("GPL v2");
