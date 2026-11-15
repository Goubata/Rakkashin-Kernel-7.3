/*
 * Copyright (C) 2018 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/ctype.h>
#include <linux/version.h>
#include <linux/overflow.h>
#include <linux/io.h>
#if KERNEL_VERSION(5, 10, 0) > LINUX_VERSION_CODE
#include <linux/dma-contiguous.h>
#endif
#include <linux/vmalloc.h>
#include <linux/dma-mapping.h>
#include <linux/mmi_annotate.h>
#include <linux/seq_file.h>
#include <linux/fs.h>
#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
#include <soc/qcom/minidump.h>
#endif
#endif

#define MAX_USER_STR 1024
#define DEFAULT_MEM_SIZE 4096
#define PERSIST_MAGIC_NUM 0xABCD1234

struct platform_data {
	phys_addr_t	mem_address;
	size_t		mem_size;
};

struct mem_data_t {
	unsigned int  size;
	size_t        cur_off;
	unsigned char *contents;
};

struct persist_data_t {
	unsigned int  magic_num;
	size_t        size;
	size_t        cur_off;
	unsigned char contents[];
};

static DEFINE_MUTEX(provider_lock);
static DEFINE_MUTEX(persist_lock);
static DEFINE_MUTEX(mem_lock);

static struct persist_data_t *persist_data;
static size_t persist_capacity;
static struct mem_data_t mem_data;
static int persist_unsupported = 0;

/* Views below are published together only after full provider initialization.
 * Provider operations take provider_lock; API operations take mem_lock then
 * persist_lock when both views are needed. */
struct annotate_provider {
	struct mem_data_t ram;
	struct persist_data_t *persist;
	void *mapping;
	size_t capacity;
	bool fallback;
	struct proc_dir_entry *file, *mem_file;
#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
	struct md_region md;
	bool md_registered;
#endif
};
static struct annotate_provider *active_provider;
static struct annotate_provider *retired_provider;

static int mmi_annotate_seq_show(struct seq_file *f, void *ptr)
{
	mutex_lock(&mem_lock);
	if (mem_data.contents && mem_data.size && mem_data.cur_off > 0) {
		seq_write(f, mem_data.contents,
			  strnlen(mem_data.contents,
				  min_t(size_t, mem_data.cur_off, mem_data.size - 1)));
	} else {
		seq_printf(f, "No annotated data.\n");
	}
	mutex_unlock(&mem_lock);

	return 0;
}

static int mmi_annotate_open(struct inode *inode, struct file *file)
{
	return single_open(file, mmi_annotate_seq_show, inode->i_private);
}

static ssize_t mmi_annotate_write(struct file *file, const char __user *buf,
				size_t count, loff_t *offset)
{
	char buffer[MAX_USER_STR];
	const size_t maxlen = sizeof(buffer) - 1;

	memset(buffer, 0, sizeof(buffer));
	if (copy_from_user(buffer, buf, count > maxlen ? maxlen : count))
		return -EFAULT;

	mmi_annotate_persist("%s", buffer);

	return count;
}

#if KERNEL_VERSION(5, 10, 0) <= LINUX_VERSION_CODE
static const struct proc_ops mmi_annotate_operations = {
	.proc_open		= mmi_annotate_open,
	.proc_read		= seq_read,
	.proc_write		= mmi_annotate_write,
	.proc_lseek		= seq_lseek,
	.proc_release	= single_release,
};
#else
static const struct file_operations mmi_annotate_operations = {
	.open		= mmi_annotate_open,
	.read		= seq_read,
	.write		= mmi_annotate_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};
#endif

static ssize_t mmi_annotate_mem_write(struct file *file, const char __user *buf,
				size_t count, loff_t *offset)
{
	char buffer[MAX_USER_STR];
	const size_t maxlen = sizeof(buffer) - 1;

	memset(buffer, 0, sizeof(buffer));
	if (copy_from_user(buffer, buf, count > maxlen ? maxlen : count))
		return -EFAULT;

	mmi_annotate("%s", buffer);

	return count;
}

static const struct proc_ops mmi_annotate_mem_operations = {
	.proc_write		= mmi_annotate_mem_write,
};

static void *mmi_annotate_map_ram(phys_addr_t start, size_t size)
{
	phys_addr_t end;
	size_t total;

	if (!size || check_add_overflow(start, (phys_addr_t)size, &end) ||
	    check_add_overflow(size, (size_t)offset_in_page(start), &total))
		return NULL;
	/* The factory reservation is no-map. Do not fabricate struct page pointers
	 * or assume a linear-map alias: use the RAM mapping API and paired teardown. */
	return memremap(start, size, MEMREMAP_WB);
}

/* Header capacity comes from the checked mapped resource, never persistent
 * metadata. Preserve the prefix that fits and always reserve a terminating NUL. */
static void mmi_annotate_restore(struct annotate_provider *ctx)
{
	size_t length = READ_ONCE(ctx->persist->cur_off);

	if (READ_ONCE(ctx->persist->magic_num) == PERSIST_MAGIC_NUM &&
	    length <= ctx->capacity) {
		length = min_t(size_t, length, ctx->ram.size - 1);
		memcpy(ctx->ram.contents, ctx->persist->contents, length);
		ctx->ram.cur_off = length;
		ctx->ram.contents[length] = 0;
	}
}

static void mmi_annotate_publish(struct annotate_provider *ctx)
{
	mutex_lock(&mem_lock);
	mutex_lock(&persist_lock);
	mem_data = ctx ? ctx->ram : (struct mem_data_t){};
	persist_data = ctx ? ctx->persist : NULL;
	persist_capacity = ctx ? ctx->capacity : 0;
	persist_unsupported = ctx ? ctx->fallback : 0;
	mutex_unlock(&persist_lock);
	mutex_unlock(&mem_lock);
}

static void mmi_annotate_remove_files(struct annotate_provider *ctx)
{
	proc_remove(ctx->file);
	proc_remove(ctx->mem_file);
	ctx->file = ctx->mem_file = NULL;
}

/* Called with provider_lock. Failed dump-table removal retains all
 * published addresses. A later probe/exit retries; never free an uncertain
 * registration's backing memory, even on a terminal module unload failure. */
static int mmi_annotate_release(struct annotate_provider *ctx)
{
#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
	int ret;

	if (ctx->md_registered) {
		ret = msm_minidump_remove_region(&ctx->md);
		if (ret)
			return ret;
		ctx->md_registered = false;
	}
#endif
	if (ctx->mapping)
		memunmap(ctx->mapping);
	kfree(ctx->ram.contents);
	kfree(ctx);
	return 0;
}

static int mmi_annotate_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct annotate_provider *ctx;
	struct resource res;
	struct device_node *node;
	size_t region_size;
	int ret;

	mutex_lock(&provider_lock);
	if (active_provider) {
		ret = -EBUSY;
		goto unlock;
	}
	if (retired_provider) {
		ret = mmi_annotate_release(retired_provider);
		if (ret)
			goto unlock;
		retired_provider = NULL;
	}
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		ret = -ENOMEM;
		goto unlock;
	}
	node = of_parse_phandle(dev->of_node, "memory-region", 0);
	if (!node) {
		ret = -EINVAL;
		goto release;
	}
	ret = of_address_to_resource(node, 0, &res);
	of_node_put(node);
	if (ret)
		goto release;
	region_size = resource_size(&res);
	if (region_size <= sizeof(*ctx->persist) ||
	    !IS_ALIGNED(res.start, __alignof__(struct persist_data_t))) {
		ret = -EINVAL;
		goto release;
	}
	ctx->capacity = region_size - sizeof(*ctx->persist);
	ctx->ram.size = DEFAULT_MEM_SIZE;
	of_property_read_u32(dev->of_node, "mem-size", &ctx->ram.size);
	if (!ctx->ram.size) {
		ret = -EINVAL;
		goto release;
	}
	ctx->fallback = of_property_read_bool(dev->of_node, "persist_unsupported");
	ctx->persist = mmi_annotate_map_ram(res.start, region_size);
	if (!ctx->persist) {
		ret = -ENOMEM;
		goto release;
	}
	ctx->mapping = ctx->persist;
	ctx->ram.contents = kzalloc(ctx->ram.size, GFP_KERNEL);
	if (!ctx->ram.contents) {
		ret = -ENOMEM;
		goto release;
	}
	mmi_annotate_restore(ctx);
	ctx->file = proc_create("driver/mmi_annotate", 0444, NULL,
				&mmi_annotate_operations);
	if (!ctx->file) {
		ret = -ENOMEM;
		goto release;
	}
	ctx->mem_file = proc_create("driver/mmi_annotate_mem", 0640, NULL,
				    &mmi_annotate_mem_operations);
	if (!ctx->mem_file) {
		ret = -ENOMEM;
		goto release;
	}
#if IS_ENABLED(CONFIG_QCOM_MINIDUMP)
	strscpy(ctx->md.name, "ANNOTATE", sizeof(ctx->md.name));
	if (!ctx->fallback) {
		ctx->md.virt_addr = (uintptr_t)ctx->persist;
		ctx->md.phys_addr = res.start;
		ctx->md.size = region_size;
	} else {
		ctx->md.virt_addr = (uintptr_t)ctx->ram.contents;
		ctx->md.phys_addr = virt_to_phys(ctx->ram.contents);
		ctx->md.size = ctx->ram.size;
	}
	ret = msm_minidump_add_region(&ctx->md);
	if (ret < 0)
		goto release;
	ctx->md_registered = true;
#endif
	/* Do not erase the old boot log until all acquisitions have succeeded. */
	ctx->persist->size = ctx->capacity;
	ctx->persist->magic_num = PERSIST_MAGIC_NUM;
	ctx->persist->cur_off = 0;
	ctx->persist->contents[0] = 0;
	mmi_annotate_publish(ctx);
	active_provider = ctx;
	platform_set_drvdata(pdev, ctx);
	mutex_unlock(&provider_lock);
	return 0;

release:
	mmi_annotate_remove_files(ctx);
	/* No fallible operation follows a successful minidump add above. */
	mmi_annotate_release(ctx);
unlock:
	mutex_unlock(&provider_lock);
	return ret;
}

int mmi_annotate(const char *fmt, ...)
{
	va_list args;
	int len = 0;
	char line_buf[512];

	if (!fmt)
		return -EINVAL;
	va_start(args, fmt);
	len = vscnprintf(line_buf, sizeof(line_buf), fmt, args);
	va_end(args);

	mutex_lock(&mem_lock);
	if (mem_data.contents && mem_data.cur_off < mem_data.size &&
	    len < mem_data.size - mem_data.cur_off) {
		memcpy(mem_data.contents + mem_data.cur_off, line_buf, len);
		mem_data.cur_off += len;
		mem_data.contents[mem_data.cur_off] = 0;
	}
	mutex_unlock(&mem_lock);

	return 0;
}
EXPORT_SYMBOL(mmi_annotate);

int mmi_annotate_persist(const char *fmt, ...)
{
	va_list args;
	size_t capacity;
	int len = 0;
	char line_buf[512];

	if (!fmt)
		return -EINVAL;
	va_start(args, fmt);
	len = vscnprintf(line_buf, sizeof(line_buf), fmt, args);
	va_end(args);

	mutex_lock(&mem_lock);
	mutex_lock(&persist_lock);
	capacity = persist_data ? min(persist_capacity, persist_data->size) : 0;
	if (persist_unsupported) {
		if (mem_data.contents && mem_data.cur_off < mem_data.size &&
		    len < mem_data.size - mem_data.cur_off) {
			memcpy(mem_data.contents + mem_data.cur_off, line_buf, len);
			mem_data.cur_off += len;
			mem_data.contents[mem_data.cur_off] = 0;
		}
	} else if (persist_data && persist_data->cur_off < capacity &&
		   len < capacity - persist_data->cur_off) {
		memcpy(persist_data->contents + persist_data->cur_off, line_buf, len);
		persist_data->cur_off += len;
		persist_data->contents[persist_data->cur_off] = 0;
	}
	mutex_unlock(&persist_lock);
	mutex_unlock(&mem_lock);

	return 0;
}
EXPORT_SYMBOL(mmi_annotate_persist);

static void mmi_annotate_remove(struct platform_device *pdev)
{
	struct annotate_provider *ctx;
	int ret;

	mutex_lock(&provider_lock);
	ctx = platform_get_drvdata(pdev);
	if (!ctx) {
		mutex_unlock(&provider_lock);
		return;
	}
	mmi_annotate_remove_files(ctx);
	mmi_annotate_publish(NULL);
	active_provider = NULL;
	platform_set_drvdata(pdev, NULL);
	ret = mmi_annotate_release(ctx);
	if (ret) {
		retired_provider = ctx;
		dev_err(&pdev->dev, "minidump removal failed (%d); retaining backing memory\n", ret);
	}
	mutex_unlock(&provider_lock);
}

static const struct of_device_id mmi_annotate_match[] = {
	{ .compatible = "mmi,annotate" },
	{}
};

static struct platform_driver mmi_annotate_driver = {
	.probe		= mmi_annotate_probe,
	.remove		= mmi_annotate_remove,
	.driver		= {
		.name = "mmi_annotate",
		.of_match_table = mmi_annotate_match,
	},
};

static int mmi_annotate_init(void)
{
	return platform_driver_register(&mmi_annotate_driver);
}

static void mmi_annotate_exit(void)
{
	platform_driver_unregister(&mmi_annotate_driver);
	mutex_lock(&provider_lock);
	if (retired_provider) {
		if (!mmi_annotate_release(retired_provider))
			retired_provider = NULL;
		else
			pr_err("ANNOTATE: retaining minidump backing until reboot\n");
	}
	mutex_unlock(&provider_lock);
}

module_init(mmi_annotate_init);
module_exit(mmi_annotate_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MMI annotate");
