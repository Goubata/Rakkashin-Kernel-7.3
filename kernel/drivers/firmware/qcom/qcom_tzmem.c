// SPDX-License-Identifier: GPL-2.0-only
/*
 * Memory allocator for buffers shared with the TrustZone.
 *
 * Copyright (C) 2023-2024 Linaro Ltd.
 */

#include <linux/bug.h>
#include <linux/cleanup.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/firmware/qcom/qcom_tzmem.h>
#include <linux/mutex.h>
#include <linux/qtee_shmbridge.h>
#include <linux/sizes.h>
#include <linux/genalloc.h>
#include <linux/gfp.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/once.h>
#include <linux/radix-tree.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>

#include "qcom_scm.h"
#include "qcom_tzmem.h"

struct qcom_tzmem_area {
	struct list_head list;
	void *vaddr;
	dma_addr_t paddr;
	size_t size;
	void *priv;
};

struct qcom_tzmem_pool {
	struct gen_pool *genpool;
	struct list_head areas;
	enum qcom_tzmem_policy policy;
	size_t increment;
	size_t max_size;
	spinlock_t lock;
	bool is_cached;
};

struct qcom_tzmem_chunk {
	size_t size;
	struct qcom_tzmem_pool *owner;
};

static struct device *qcom_tzmem_dev;
static RADIX_TREE(qcom_tzmem_chunks, GFP_ATOMIC);
static DEFINE_SPINLOCK(qcom_tzmem_chunks_lock);

#if IS_ENABLED(CONFIG_QCOM_TZMEM_MODE_GENERIC)

static int qcom_tzmem_init(void)
{
	return 0;
}

static int qcom_tzmem_init_area(struct qcom_tzmem_area *area)
{
	return 0;
}

static void qcom_tzmem_cleanup_area(struct qcom_tzmem_area *area)
{

}

bool qcom_tzmem_get_status(void)
{
	return false;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_get_status);

int qcom_tzmem_query(phys_addr_t paddr)
{
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_query);

int qcom_tzmem_shm_bridge_create_with_vmid(phys_addr_t paddr, size_t size,
					   u32 vmid, u64 *handle)
{
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_shm_bridge_create_with_vmid);

#elif IS_ENABLED(CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE)

#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/of.h>

#define QCOM_SHM_BRIDGE_NUM_VM_SHIFT 9
#define QCOM_SHM_BRIDGE_SELF_OWNER_BIT 1

static bool qcom_tzmem_using_shm_bridge;

/*
 * Registry of live bridges (android16-6.12 parity): vendor clients register the
 * same buffer more than once and expect the existing handle plus a reference.
 */
struct qcom_tzmem_bridge {
	struct list_head list;
	phys_addr_t paddr;
	u64 handle;
	unsigned int refs;
};

static LIST_HEAD(qcom_tzmem_bridges);
static DEFINE_MUTEX(qcom_tzmem_bridges_lock);

static struct qcom_tzmem_bridge *qcom_tzmem_bridge_by_paddr(phys_addr_t paddr)
{
	struct qcom_tzmem_bridge *b;

	lockdep_assert_held(&qcom_tzmem_bridges_lock);
	list_for_each_entry(b, &qcom_tzmem_bridges, list)
		if (b->paddr == paddr)
			return b;
	return NULL;
}

bool qcom_tzmem_get_status(void)
{
	return qcom_tzmem_using_shm_bridge;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_get_status);

/* -EEXIST if a bridge starting at @paddr exists, else 0 */
int qcom_tzmem_query(phys_addr_t paddr)
{
	if (!qcom_tzmem_using_shm_bridge)
		return 0;

	guard(mutex)(&qcom_tzmem_bridges_lock);
	return qcom_tzmem_bridge_by_paddr(paddr) ? -EEXIST : 0;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_query);

/* List of machines that are known to not support SHM bridge correctly. */
static const char *const qcom_tzmem_blacklist[] = {
	"qcom,sc7180", /* hang in rmtfs memory assignment */
	"qcom,sc8180x",
	"qcom,sdm670", /* failure in GPU firmware loading */
	"qcom,sdm845", /* reset in rmtfs memory assignment */
	"qcom,sm7150", /* reset in rmtfs memory assignment */
	"qcom,sm8150", /* reset in rmtfs memory assignment */
	NULL
};

static int qcom_tzmem_init(void)
{
	const char *const *platform;
	int ret;

	for (platform = qcom_tzmem_blacklist; *platform; platform++) {
		if (of_machine_is_compatible(*platform))
			goto notsupp;
	}

	ret = qcom_scm_shm_bridge_enable(qcom_tzmem_dev);
	if (ret == -EOPNOTSUPP)
		goto notsupp;

	if (!ret)
		qcom_tzmem_using_shm_bridge = true;

	return ret;

notsupp:
	dev_info(qcom_tzmem_dev, "SHM Bridge not supported\n");
	return 0;
}

/**
 * qcom_tzmem_shm_bridge_create() - Create a SHM bridge.
 * @paddr: Physical address of the memory to share.
 * @size: Size of the memory to share.
 * @handle: Handle to the SHM bridge.
 *
 * On platforms that support SHM bridge, this function creates a SHM bridge
 * for the given memory region with QTEE. The handle returned by this function
 * must be passed to qcom_tzmem_shm_bridge_delete() to free the SHM bridge.
 *
 * Return: On success, returns 0; on failure, returns < 0.
 */
int qcom_tzmem_shm_bridge_create_with_vmid(phys_addr_t paddr, size_t size,
					   u32 vmid, u64 *handle)
{
	u64 pfn_and_ns_perm, ipfn_and_s_perm, size_and_flags;
	struct qcom_tzmem_bridge *b;
	int ret;

	if (!qcom_tzmem_using_shm_bridge)
		return 0;

	size = PAGE_ALIGN(size);

	guard(mutex)(&qcom_tzmem_bridges_lock);
	b = qcom_tzmem_bridge_by_paddr(paddr);
	if (b) {
		b->refs++;
		*handle = b->handle;
		return 0;
	}

	b = kzalloc_obj(*b);
	if (!b)
		return -ENOMEM;

	pfn_and_ns_perm = paddr;
	ipfn_and_s_perm = paddr | QCOM_SCM_PERM_RW;
	if (vmid) {
		/* shared with one NS VM (@vmid) */
		size_and_flags = size | (1 << QCOM_SHM_BRIDGE_NUM_VM_SHIFT);
		pfn_and_ns_perm |= QCOM_SCM_PERM_RW;
	} else {
		/* self-owned by the caller's VM, RW (6.12 default bridge) */
		size_and_flags = size | (QCOM_SHM_BRIDGE_SELF_OWNER_BIT << 1) |
				 (QCOM_SCM_PERM_RW << 2);
	}

	ret = qcom_scm_shm_bridge_create(pfn_and_ns_perm, ipfn_and_s_perm,
					 size_and_flags, vmid, handle);
	if (ret) {
		dev_err(qcom_tzmem_dev,
			"SHM Bridge failed: ret %d paddr 0x%pa, size %zu vmid %#x\n",
			ret, &paddr, size, vmid);
		kfree(b);
		return -EINVAL;
	}

	b->paddr = paddr;
	b->handle = *handle;
	b->refs = 1;
	list_add_tail(&b->list, &qcom_tzmem_bridges);
	return 0;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_shm_bridge_create_with_vmid);

int qcom_tzmem_shm_bridge_create(phys_addr_t paddr, size_t size, u64 *handle)
{
	return qcom_tzmem_shm_bridge_create_with_vmid(paddr, size, 0, handle);
}
EXPORT_SYMBOL_GPL(qcom_tzmem_shm_bridge_create);

/**
 * qcom_tzmem_shm_bridge_delete() - Delete a SHM bridge.
 * @handle: Handle to the SHM bridge.
 *
 * On platforms that support SHM bridge, this function deletes the SHM bridge
 * for the given memory region. The handle must be the same as the one
 * returned by qcom_tzmem_shm_bridge_create().
 */
void qcom_tzmem_shm_bridge_delete(u64 handle)
{
	struct qcom_tzmem_bridge *b;

	if (!qcom_tzmem_using_shm_bridge)
		return;

	guard(mutex)(&qcom_tzmem_bridges_lock);
	list_for_each_entry(b, &qcom_tzmem_bridges, list) {
		if (b->handle != handle)
			continue;
		if (--b->refs == 0) {
			qcom_scm_shm_bridge_delete(handle);
			list_del(&b->list);
			kfree(b);
		}
		return;
	}
	pr_err("qcom_tzmem: unknown SHM bridge handle %#llx\n", handle);
}
EXPORT_SYMBOL_GPL(qcom_tzmem_shm_bridge_delete);

static int qcom_tzmem_init_area(struct qcom_tzmem_area *area)
{
	int ret;

	u64 *handle __free(kfree) = kzalloc_obj(*handle);
	if (!handle)
		return -ENOMEM;

	ret = qcom_tzmem_shm_bridge_create(area->paddr, area->size, handle);
	if (ret)
		return ret;

	area->priv = no_free_ptr(handle);

	return 0;
}

static void qcom_tzmem_cleanup_area(struct qcom_tzmem_area *area)
{
	u64 *handle = area->priv;

	qcom_tzmem_shm_bridge_delete(*handle);
	kfree(handle);
}

#endif /* CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE */

static void qcom_tzmem_free_area_mem(struct qcom_tzmem_pool *pool,
				     struct qcom_tzmem_area *area)
{
	if (pool->is_cached) {
		dma_unmap_single(qcom_tzmem_dev, area->paddr, area->size,
				 DMA_TO_DEVICE);
		free_pages((unsigned long)area->vaddr, get_order(area->size));
	} else {
		dma_free_coherent(qcom_tzmem_dev, area->size, area->vaddr,
				  area->paddr);
	}
}

static int qcom_tzmem_pool_add_memory(struct qcom_tzmem_pool *pool,
				      size_t size, gfp_t gfp)
{
	int ret;

	struct qcom_tzmem_area *area __free(kfree) = kzalloc_obj(*area, gfp);
	if (!area)
		return -ENOMEM;

	area->size = PAGE_ALIGN(size);

	if (pool->is_cached) {
		area->vaddr = (void *)__get_free_pages(gfp, get_order(area->size));
		if (!area->vaddr)
			return -ENOMEM;
		area->paddr = dma_map_single(qcom_tzmem_dev, area->vaddr,
					     area->size, DMA_TO_DEVICE);
		if (dma_mapping_error(qcom_tzmem_dev, area->paddr)) {
			free_pages((unsigned long)area->vaddr,
				   get_order(area->size));
			return -ENOMEM;
		}
	} else {
		area->vaddr = dma_alloc_coherent(qcom_tzmem_dev, area->size,
						 &area->paddr, gfp);
		if (!area->vaddr)
			return -ENOMEM;
	}

	ret = qcom_tzmem_init_area(area);
	if (ret) {
		qcom_tzmem_free_area_mem(pool, area);
		return ret;
	}

	ret = gen_pool_add_virt(pool->genpool, (unsigned long)area->vaddr,
				(phys_addr_t)area->paddr, size, -1);
	if (ret) {
		qcom_tzmem_cleanup_area(area);
		qcom_tzmem_free_area_mem(pool, area);
		return ret;
	}

	scoped_guard(spinlock_irqsave, &pool->lock)
		list_add_tail(&area->list, &pool->areas);

	area = NULL;
	return 0;
}

/**
 * qcom_tzmem_pool_new() - Create a new TZ memory pool.
 * @config: Pool configuration.
 *
 * Create a new pool of memory suitable for sharing with the TrustZone.
 *
 * Must not be used in atomic context.
 *
 * Return: New memory pool address or ERR_PTR() on error.
 */
struct qcom_tzmem_pool *
qcom_tzmem_pool_new(const struct qcom_tzmem_pool_config *config)
{
	int ret = -ENOMEM;

	might_sleep();

	switch (config->policy) {
	case QCOM_TZMEM_POLICY_STATIC:
		if (!config->initial_size)
			return ERR_PTR(-EINVAL);
		break;
	case QCOM_TZMEM_POLICY_MULTIPLIER:
		if (!config->increment)
			return ERR_PTR(-EINVAL);
		break;
	case QCOM_TZMEM_POLICY_ON_DEMAND:
		break;
	default:
		return ERR_PTR(-EINVAL);
	}

	struct qcom_tzmem_pool *pool __free(kfree) = kzalloc_obj(*pool);
	if (!pool)
		return ERR_PTR(-ENOMEM);

	pool->genpool = gen_pool_create(PAGE_SHIFT, -1);
	if (!pool->genpool)
		return ERR_PTR(-ENOMEM);

	gen_pool_set_algo(pool->genpool, gen_pool_best_fit, NULL);

	pool->policy = config->policy;
	pool->increment = config->increment;
	pool->max_size = config->max_size;
	pool->is_cached = config->is_cached;
	INIT_LIST_HEAD(&pool->areas);
	spin_lock_init(&pool->lock);

	if (config->initial_size) {
		ret = qcom_tzmem_pool_add_memory(pool, config->initial_size,
						 GFP_KERNEL);
		if (ret) {
			gen_pool_destroy(pool->genpool);
			return ERR_PTR(ret);
		}
	}

	return_ptr(pool);
}
EXPORT_SYMBOL_GPL(qcom_tzmem_pool_new);

/**
 * qcom_tzmem_pool_free() - Destroy a TZ memory pool and free all resources.
 * @pool: Memory pool to free.
 *
 * Must not be called if any of the allocated chunks has not been freed.
 * Must not be used in atomic context.
 */
void qcom_tzmem_pool_free(struct qcom_tzmem_pool *pool)
{
	struct qcom_tzmem_area *area, *next;
	struct qcom_tzmem_chunk *chunk;
	struct radix_tree_iter iter;
	bool non_empty = false;
	void __rcu **slot;

	might_sleep();

	if (!pool)
		return;

	scoped_guard(spinlock_irqsave, &qcom_tzmem_chunks_lock) {
		radix_tree_for_each_slot(slot, &qcom_tzmem_chunks, &iter, 0) {
			chunk = radix_tree_deref_slot_protected(slot,
						&qcom_tzmem_chunks_lock);

			if (chunk->owner == pool)
				non_empty = true;
		}
	}

	WARN(non_empty, "Freeing TZ memory pool with memory still allocated");

	list_for_each_entry_safe(area, next, &pool->areas, list) {
		list_del(&area->list);
		qcom_tzmem_cleanup_area(area);
		qcom_tzmem_free_area_mem(pool, area);
		kfree(area);
	}

	gen_pool_destroy(pool->genpool);
	kfree(pool);
}
EXPORT_SYMBOL_GPL(qcom_tzmem_pool_free);

static void devm_qcom_tzmem_pool_free(void *data)
{
	struct qcom_tzmem_pool *pool = data;

	qcom_tzmem_pool_free(pool);
}

/**
 * devm_qcom_tzmem_pool_new() - Managed variant of qcom_tzmem_pool_new().
 * @dev: Device managing this resource.
 * @config: Pool configuration.
 *
 * Must not be used in atomic context.
 *
 * Return: Address of the managed pool or ERR_PTR() on failure.
 */
struct qcom_tzmem_pool *
devm_qcom_tzmem_pool_new(struct device *dev,
			 const struct qcom_tzmem_pool_config *config)
{
	struct qcom_tzmem_pool *pool;
	int ret;

	pool = qcom_tzmem_pool_new(config);
	if (IS_ERR(pool))
		return pool;

	ret = devm_add_action_or_reset(dev, devm_qcom_tzmem_pool_free, pool);
	if (ret)
		return ERR_PTR(ret);

	return pool;
}
EXPORT_SYMBOL_GPL(devm_qcom_tzmem_pool_new);

static bool qcom_tzmem_try_grow_pool(struct qcom_tzmem_pool *pool,
				     size_t requested, gfp_t gfp)
{
	size_t current_size = gen_pool_size(pool->genpool);

	if (pool->max_size && (current_size + requested) > pool->max_size)
		return false;

	switch (pool->policy) {
	case QCOM_TZMEM_POLICY_STATIC:
		return false;
	case QCOM_TZMEM_POLICY_MULTIPLIER:
		requested = current_size * pool->increment;
		break;
	case QCOM_TZMEM_POLICY_ON_DEMAND:
		break;
	}

	return !qcom_tzmem_pool_add_memory(pool, requested, gfp);
}

/**
 * qcom_tzmem_alloc() - Allocate a memory chunk suitable for sharing with TZ.
 * @pool: TZ memory pool from which to allocate memory.
 * @size: Number of bytes to allocate.
 * @gfp: GFP flags.
 *
 * Can be used in any context.
 *
 * Return:
 * Address of the allocated buffer or NULL if no more memory can be allocated.
 * The buffer must be released using qcom_tzmem_free().
 */
void *qcom_tzmem_alloc(struct qcom_tzmem_pool *pool, size_t size, gfp_t gfp)
{
	unsigned long vaddr;
	int ret;

	if (!size)
		return NULL;

	size = PAGE_ALIGN(size);

	struct qcom_tzmem_chunk *chunk __free(kfree) = kzalloc_obj(*chunk, gfp);
	if (!chunk)
		return NULL;

again:
	vaddr = gen_pool_alloc(pool->genpool, size);
	if (!vaddr) {
		if (qcom_tzmem_try_grow_pool(pool, size, gfp))
			goto again;

		return NULL;
	}

	chunk->size = size;
	chunk->owner = pool;

	scoped_guard(spinlock_irqsave, &qcom_tzmem_chunks_lock) {
		ret = radix_tree_insert(&qcom_tzmem_chunks, vaddr, chunk);
		if (ret) {
			gen_pool_free(pool->genpool, vaddr, size);
			return NULL;
		}

		chunk = NULL;
	}

	return (void *)vaddr;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_alloc);

/**
 * qcom_tzmem_free() - Release a buffer allocated from a TZ memory pool.
 * @vaddr: Virtual address of the buffer.
 *
 * Can be used in any context.
 */
void qcom_tzmem_free(void *vaddr)
{
	struct qcom_tzmem_chunk *chunk;

	scoped_guard(spinlock_irqsave, &qcom_tzmem_chunks_lock)
		chunk = radix_tree_delete_item(&qcom_tzmem_chunks,
					       (unsigned long)vaddr, NULL);

	if (!chunk) {
		WARN(1, "Virtual address %p not owned by TZ memory allocator",
		     vaddr);
		return;
	}

	scoped_guard(spinlock_irqsave, &chunk->owner->lock)
		gen_pool_free(chunk->owner->genpool, (unsigned long)vaddr,
			      chunk->size);
	kfree(chunk);
}
EXPORT_SYMBOL_GPL(qcom_tzmem_free);

/**
 * qcom_tzmem_to_phys() - Map the virtual address of TZ memory to physical.
 * @vaddr: Virtual address of memory allocated from a TZ memory pool.
 *
 * Can be used in any context. The address must point to memory allocated
 * using qcom_tzmem_alloc().
 *
 * Returns:
 * Physical address mapped from the virtual or 0 if the mapping failed.
 */
phys_addr_t qcom_tzmem_to_phys(void *vaddr)
{
	struct qcom_tzmem_chunk *chunk;
	struct radix_tree_iter iter;
	void __rcu **slot;
	phys_addr_t ret;

	guard(spinlock_irqsave)(&qcom_tzmem_chunks_lock);

	radix_tree_for_each_slot(slot, &qcom_tzmem_chunks, &iter, 0) {
		chunk = radix_tree_deref_slot_protected(slot,
						&qcom_tzmem_chunks_lock);

		ret = gen_pool_virt_to_phys(chunk->owner->genpool,
					    (unsigned long)vaddr);
		if (ret == -1)
			continue;

		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_to_phys);

/* make CPU writes to a cached-pool buffer visible to TZ (6.12 naming kept) */
void qcom_tzmem_flush_shm_buf(phys_addr_t paddr, size_t size)
{
	dma_sync_single_for_cpu(qcom_tzmem_dev, paddr, size, DMA_FROM_DEVICE);
}
EXPORT_SYMBOL_GPL(qcom_tzmem_flush_shm_buf);

void qcom_tzmem_inv_shm_buf(phys_addr_t paddr, size_t size)
{
	dma_sync_single_for_device(qcom_tzmem_dev, paddr, size, DMA_TO_DEVICE);
}
EXPORT_SYMBOL_GPL(qcom_tzmem_inv_shm_buf);

/* ---- <linux/qtee_shmbridge.h> (qcom-soc qtee_shmbridge.c) ---- */
static struct qcom_tzmem_pool *qtee_shmbridge_pool;
static DEFINE_MUTEX(qtee_shmbridge_pool_lock);

static struct qcom_tzmem_pool *qtee_shmbridge_get_pool(void)
{
	struct qcom_tzmem_pool_config cfg = {
		.initial_size = SZ_1M,
		.policy = QCOM_TZMEM_POLICY_ON_DEMAND,
		.max_size = SZ_8M,
		.is_cached = true,
	};
	struct qcom_tzmem_pool *pool;

	guard(mutex)(&qtee_shmbridge_pool_lock);
	if (qtee_shmbridge_pool)
		return qtee_shmbridge_pool;
	if (!qcom_tzmem_dev)
		return ERR_PTR(-EPROBE_DEFER);
	pool = qcom_tzmem_pool_new(&cfg);
	if (!IS_ERR(pool))
		qtee_shmbridge_pool = pool;
	return pool;
}

bool qtee_shmbridge_is_enabled(void)
{
	return qcom_tzmem_get_status();
}
EXPORT_SYMBOL(qtee_shmbridge_is_enabled);

int32_t qtee_shmbridge_query(phys_addr_t paddr)
{
	return qcom_tzmem_query(paddr);
}
EXPORT_SYMBOL(qtee_shmbridge_query);

int32_t qtee_shmbridge_register(phys_addr_t paddr, size_t size,
				uint32_t *ns_vmid_list, uint32_t *ns_vm_perm_list,
				uint32_t ns_vmid_num, uint32_t tz_perm,
				uint64_t *handle)
{
	if (!handle || !ns_vmid_list || !ns_vm_perm_list)
		return -EINVAL;

	return qcom_tzmem_shm_bridge_create_with_vmid(paddr, size,
						      ns_vmid_list[0], handle);
}
EXPORT_SYMBOL(qtee_shmbridge_register);

int32_t qtee_shmbridge_deregister(uint64_t handle)
{
	qcom_tzmem_shm_bridge_delete(handle);
	return 0;
}
EXPORT_SYMBOL(qtee_shmbridge_deregister);

int32_t qtee_shmbridge_allocate_shm(size_t size, struct qtee_shm *shm)
{
	struct qcom_tzmem_pool *pool = qtee_shmbridge_get_pool();

	if (IS_ERR(pool))
		return PTR_ERR(pool);

	shm->size = PAGE_ALIGN(size);
	shm->vaddr = qcom_tzmem_alloc(pool, shm->size, GFP_KERNEL);
	if (!shm->vaddr)
		return -ENOMEM;
	shm->paddr = qcom_tzmem_to_phys(shm->vaddr);
	return 0;
}
EXPORT_SYMBOL(qtee_shmbridge_allocate_shm);

void qtee_shmbridge_free_shm(struct qtee_shm *shm)
{
	if (IS_ERR_OR_NULL(shm) || !shm->vaddr)
		return;
	qcom_tzmem_free(shm->vaddr);
}
EXPORT_SYMBOL(qtee_shmbridge_free_shm);

void qtee_shmbridge_flush_shm_buf(struct qtee_shm *shm)
{
	qcom_tzmem_flush_shm_buf(shm->paddr, shm->size);
}
EXPORT_SYMBOL(qtee_shmbridge_flush_shm_buf);

void qtee_shmbridge_inv_shm_buf(struct qtee_shm *shm)
{
	qcom_tzmem_inv_shm_buf(shm->paddr, shm->size);
}
EXPORT_SYMBOL(qtee_shmbridge_inv_shm_buf);

static void qcom_tzmem_do_init(int *result)
{
	*result = qcom_tzmem_init();
}

int qcom_tzmem_enable(struct device *dev)
{
	static int result;

	qcom_tzmem_dev = dev;
	DO_ONCE(qcom_tzmem_do_init, &result);
	return result;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_enable);

MODULE_DESCRIPTION("TrustZone memory allocator for Qualcomm firmware drivers");
MODULE_AUTHOR("Bartosz Golaszewski <bartosz.golaszewski@linaro.org>");
MODULE_LICENSE("GPL");
