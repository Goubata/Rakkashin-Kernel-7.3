#!/usr/bin/env python3
"""Restore the android16-6.12 dma-heap pool accounting on 7.3.

6.12 (Motorola kernel-common 479a9e4ae) had dma_heap_ops.get_pool_size,
dma_heap_try_get_pool_size_kb() and /sys/kernel/dma_heap/total_pools_kb, which
Android libmeminfo reads for "DMA-BUF heap pool" accounting; qcom_dma_heaps
implements get_pool_size. android18-7.3 dropped all three.
"""
from pathlib import Path
import sys
SRC=Path(sys.argv[1] if len(sys.argv)>1 else Path(__file__).resolve().parents[2]/'avr-upstream-audit/android-7.3')

def edit(rel,old,new):
    p=SRC/rel;s=p.read_text()
    if new in s:return
    if s.count(old)!=1:raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old,new,1))

edit('include/linux/dma-heap.h','''				    u32 fd_flags,
				    u64 heap_flags);
};''','''				    u32 fd_flags,
				    u64 heap_flags);
	/* ANDROID: if heap maintains memory pools, get pool size in bytes */
	long (*get_pool_size)(struct dma_heap *heap);
};''')
edit('include/linux/dma-heap.h','#endif /* _DMA_HEAPS_H */','''/**
 * dma_heap_try_get_pool_size_kb - Returns total dma-heap pool size in kb
 * if there is no lock contention. The pool size will always be 0 if no heaps
 * use pools, or do not implement get_pool_size.
 **/
long dma_heap_try_get_pool_size_kb(void);
#endif /* _DMA_HEAPS_H */''')
D='drivers/dma-buf/dma-heap.c'
edit(D,'''static char *dma_heap_devnode(const struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "dma_heap/%s", dev_name(dev));
}
''','''static char *dma_heap_devnode(const struct device *dev, umode_t *mode)
{
	return kasprintf(GFP_KERNEL, "dma_heap/%s", dev_name(dev));
}

long dma_heap_try_get_pool_size_kb(void)
{
	struct dma_heap *heap;
	u64 total_pool_size = 0;

	if (!mutex_trylock(&heap_list_lock))
		return -1;

	list_for_each_entry(heap, &heap_list, list) {
		if (heap->ops->get_pool_size)
			total_pool_size += heap->ops->get_pool_size(heap);
	}
	mutex_unlock(&heap_list_lock);

	return (long)(total_pool_size / 1024);
}
EXPORT_SYMBOL_GPL(dma_heap_try_get_pool_size_kb);

static ssize_t total_pools_kb_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	struct dma_heap *heap;
	u64 total_pool_size = 0;

	mutex_lock(&heap_list_lock);
	list_for_each_entry(heap, &heap_list, list) {
		if (heap->ops->get_pool_size)
			total_pool_size += heap->ops->get_pool_size(heap);
	}
	mutex_unlock(&heap_list_lock);

	return sysfs_emit(buf, "%llu\\n", total_pool_size / 1024);
}

static struct kobj_attribute total_pools_kb_attr =
	__ATTR_RO(total_pools_kb);

static struct attribute *dma_heap_sysfs_attrs[] = {
	&total_pools_kb_attr.attr,
	NULL,
};

ATTRIBUTE_GROUPS(dma_heap_sysfs);

static struct kobject *dma_heap_kobject;

static int dma_heap_sysfs_setup(void)
{
	int ret;

	dma_heap_kobject = kobject_create_and_add("dma_heap", kernel_kobj);
	if (!dma_heap_kobject)
		return -ENOMEM;

	ret = sysfs_create_groups(dma_heap_kobject, dma_heap_sysfs_groups);
	if (ret) {
		kobject_put(dma_heap_kobject);
		return ret;
	}

	return 0;
}

static void dma_heap_sysfs_teardown(void)
{
	kobject_put(dma_heap_kobject);
}
''')
edit(D,'''static int dma_heap_init(void)
{
	int ret;

	ret = alloc_chrdev_region(&dma_heap_devt, 0, NUM_HEAP_MINORS, DEVNAME);
	if (ret)
		return ret;

	dma_heap_class = class_create(DEVNAME);
	if (IS_ERR(dma_heap_class)) {
		unregister_chrdev_region(dma_heap_devt, NUM_HEAP_MINORS);
		return PTR_ERR(dma_heap_class);
	}
	dma_heap_class->devnode = dma_heap_devnode;

	return 0;
}''','''static int dma_heap_init(void)
{
	int ret;

	ret = dma_heap_sysfs_setup();
	if (ret)
		return ret;

	ret = alloc_chrdev_region(&dma_heap_devt, 0, NUM_HEAP_MINORS, DEVNAME);
	if (ret)
		goto err_chrdev;

	dma_heap_class = class_create(DEVNAME);
	if (IS_ERR(dma_heap_class)) {
		ret = PTR_ERR(dma_heap_class);
		goto err_class;
	}
	dma_heap_class->devnode = dma_heap_devnode;

	return 0;

err_class:
	unregister_chrdev_region(dma_heap_devt, NUM_HEAP_MINORS);
err_chrdev:
	dma_heap_sysfs_teardown();
	return ret;
}''')
s=(SRC/D).read_text()
if '#include <linux/kobject.h>' not in s:
    s=s.replace('#include <linux/list.h>\n','#include <linux/kobject.h>\n#include <linux/list.h>\n',1)
    (SRC/D).write_text(s)
print('applied dma-heap pool accounting to',SRC)
