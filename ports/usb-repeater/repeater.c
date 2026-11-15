// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/usb/repeater.h>

static LIST_HEAD(repeater_list);
static DEFINE_SPINLOCK(repeater_lock);

void usb_put_repeater(struct usb_repeater *r)
{
	if (r) {
		struct device *dev = r->dev;
		struct module *owner = dev->driver ? dev->driver->owner : NULL;

		put_device(dev);
		module_put(owner);
	}
}
EXPORT_SYMBOL_GPL(usb_put_repeater);

static struct usb_repeater *of_usb_find_repeater(struct device_node *node)
{
	struct usb_repeater *r;

	if (!node)
		return ERR_PTR(-EINVAL);
	if (!of_device_is_available(node))
		return ERR_PTR(-ENODEV);

	list_for_each_entry(r, &repeater_list, head) {
		if (node != r->dev->of_node)
			continue;
		return r;
	}

	return ERR_PTR(-EPROBE_DEFER);
}

struct repeater_reference {
	struct device *dev;
	struct module *owner;
};

static void devm_usb_repeater_release_by_node(struct device *dev, void *res)
{
	struct repeater_reference *reference = res;

	/* Release the exact acquired references; never read a removed provider. */
	put_device(reference->dev);
	module_put(reference->owner);
}

struct usb_repeater *devm_usb_get_repeater_by_node(struct device *dev,
			struct device_node *node)
{
	struct usb_repeater *r = ERR_PTR(-ENOMEM);
	struct repeater_reference *r_devm;
	struct module *owner;
	unsigned long flags;

	r_devm = devres_alloc(devm_usb_repeater_release_by_node,
					sizeof(*r_devm), GFP_KERNEL);
	if (!r_devm) {
		dev_dbg(dev, "failed to allocate memory for r_devm\n");
		return r;
	}

	spin_lock_irqsave(&repeater_lock, flags);
	r = of_usb_find_repeater(node);
	if (IS_ERR(r)) {
		devres_free(r_devm);
		goto err0;
	}

	if (!r->dev->driver) {
		r = ERR_PTR(-ENODEV);
		devres_free(r_devm);
		goto err0;
	}
	owner = r->dev->driver->owner;
	if (!try_module_get(owner)) {
		r = ERR_PTR(-ENODEV);
		devres_free(r_devm);
		goto err0;
	}

	r_devm->dev = get_device(r->dev);
	r_devm->owner = owner;
	devres_add(dev, r_devm);
err0:
	spin_unlock_irqrestore(&repeater_lock, flags);
	return r;
}
EXPORT_SYMBOL_GPL(devm_usb_get_repeater_by_node);

struct usb_repeater *devm_usb_get_repeater_by_phandle(struct device *dev,
	const char *phandle, u8 index)
{
	struct device_node *node;
	struct usb_repeater *r;

	if (!dev->of_node) {
		dev_dbg(dev, "device does not have a device node entry\n");
		return ERR_PTR(-EINVAL);
	}

	node = of_parse_phandle(dev->of_node, phandle, index);
	if (!node) {
		dev_dbg(dev, "failed to get %s phandle in %pOF node\n", phandle,
			dev->of_node);
		return ERR_PTR(-ENODEV);
	}

	r = devm_usb_get_repeater_by_node(dev, node);
	of_node_put(node);
	return r;
}
EXPORT_SYMBOL_GPL(devm_usb_get_repeater_by_phandle);

struct usb_repeater *devm_usb_get_optional_repeater_by_phandle(
			struct device *dev, const char *phandle, u8 index)
{
	struct device_node *node;
	struct usb_repeater *r;

	if (!dev->of_node) {
		dev_dbg(dev, "device does not have a device node entry\n");
		return ERR_PTR(-EINVAL);
	}

	node = of_parse_phandle(dev->of_node, phandle, index);
	if (!node) {
		dev_dbg(dev, "optional phandle '%s' not found in %pOF node\n",
			phandle, dev->of_node);
		return NULL;
	}

	r = devm_usb_get_repeater_by_node(dev, node);
	of_node_put(node);
	return r;
}
EXPORT_SYMBOL_GPL(devm_usb_get_optional_repeater_by_phandle);

struct usb_repeater *usb_get_repeater_by_node(struct device_node *node)
{
	struct usb_repeater *r = ERR_PTR(-ENODEV);
	unsigned long flags;

	spin_lock_irqsave(&repeater_lock, flags);
	r = of_usb_find_repeater(node);
	spin_unlock_irqrestore(&repeater_lock, flags);

	return r;
}
EXPORT_SYMBOL_GPL(usb_get_repeater_by_node);

struct usb_repeater *usb_get_repeater_by_phandle(struct device *dev,
	const char *phandle, u8 index)
{
	struct device_node *node;
	struct usb_repeater *r;

	if (!dev->of_node) {
		dev_dbg(dev, "device does not have a device node entry\n");
		return ERR_PTR(-EINVAL);
	}

	node = of_parse_phandle(dev->of_node, phandle, index);
	if (!node) {
		dev_dbg(dev, "failed to get %s phandle in %pOF node\n", phandle,
			dev->of_node);
		return ERR_PTR(-ENODEV);
	}

	r = usb_get_repeater_by_node(node);
	of_node_put(node);
	return r;
}
EXPORT_SYMBOL_GPL(usb_get_repeater_by_phandle);

/**
 * usb_add_repeater_dev - Add repeater device
 * @r: repeater device available
 *
 * It shall used by any USB repeater driver to register
 * device to allow USB PHY driver to get access to it
 * for invoking repeater specific operations.
 */
int usb_add_repeater_dev(struct usb_repeater *r)
{
	struct usb_repeater *existing;
	unsigned long flags;

	if (!r || !r->dev || !r->dev->of_node)
		return -EINVAL;
	spin_lock_irqsave(&repeater_lock, flags);
	list_for_each_entry(existing, &repeater_list, head) {
		if (existing == r || existing->dev->of_node == r->dev->of_node) {
			spin_unlock_irqrestore(&repeater_lock, flags);
			return -EEXIST;
		}
	}
	list_add_tail(&r->head, &repeater_list);
	spin_unlock_irqrestore(&repeater_lock, flags);
	return 0;
}
EXPORT_SYMBOL_GPL(usb_add_repeater_dev);

/**
 * usb_remove_repeater_dev - remove repeater device
 * @r: USB repeater device to remove from repeater list
 */
void usb_remove_repeater_dev(struct usb_repeater *r)
{
	unsigned long flags;

	spin_lock_irqsave(&repeater_lock, flags);
	if (r) {
		struct usb_repeater *existing;

		list_for_each_entry(existing, &repeater_list, head) {
			if (existing == r) {
				list_del_init(&r->head);
				break;
			}
		}
	}
	spin_unlock_irqrestore(&repeater_lock, flags);
}
EXPORT_SYMBOL_GPL(usb_remove_repeater_dev);

MODULE_DESCRIPTION("USB repeater framework");
MODULE_LICENSE("GPL");
