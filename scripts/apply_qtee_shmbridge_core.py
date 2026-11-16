#!/usr/bin/env python3
"""Restore the android16-6.12 (qcom-soc) TZ shared-memory bridge API on 7.3.

The vendor TEE stack (si_core, smcinvoke, qseecom, hdcp, kgsl tz governor,
fastrpc) uses <linux/qtee_shmbridge.h>. On the phone's 6.12 kernel that is a
thin layer over qcom_tzmem (qcom-soc drivers/firmware/qcom/qtee_shmbridge.c)
plus these qcom_tzmem features that android18-7.3 does not have:
  - cached pools (pool_config.is_cached: page allocator + streaming DMA map)
  - bridge registry: re-registering a paddr returns the existing handle and
    takes a reference; qcom_tzmem_query() reports -EEXIST for a bridged paddr
  - qcom_tzmem_shm_bridge_create_with_vmid(); vmid 0 = self-owned bridge, the
    flag encoding this phone's TZ runs with on 6.12 (7.3 upstream uses a
    one-VM HLOS bridge instead)
  - qcom_tzmem_flush_shm_buf()/qcom_tzmem_inv_shm_buf()
The qtee_shmbridge_* exports live in qcom_tzmem.ko here (qcom-scm.ko on 6.12);
the default pool (1 MiB, on-demand to 8 MiB, cached) is created on first use.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
C = 'drivers/firmware/qcom/qcom_tzmem.c'
H = 'include/linux/firmware/qcom/qcom_tzmem.h'


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:70]!r}')
    p.write_text(s.replace(old, new, 1))


# ---- header ----
edit(H, ''' * @max_size: Size above which the pool will never grow.
 */
struct qcom_tzmem_pool_config {
	size_t initial_size;
	enum qcom_tzmem_policy policy;
	size_t increment;
	size_t max_size;
};''', ''' * @max_size: Size above which the pool will never grow.
 * @is_cached: Back the pool with cached pages (callers flush/invalidate).
 */
struct qcom_tzmem_pool_config {
	size_t initial_size;
	enum qcom_tzmem_policy policy;
	size_t increment;
	size_t max_size;
	bool is_cached;
};''')
edit(H, '''phys_addr_t qcom_tzmem_to_phys(void *ptr);
''', '''phys_addr_t qcom_tzmem_to_phys(void *ptr);

/* ANDROID: vendor shmbridge support (android16-6.12 parity) */
bool qcom_tzmem_get_status(void);
int qcom_tzmem_query(phys_addr_t paddr);
int qcom_tzmem_shm_bridge_create_with_vmid(phys_addr_t paddr, size_t size,
					   u32 vmid, u64 *handle);
void qcom_tzmem_flush_shm_buf(phys_addr_t paddr, size_t size);
void qcom_tzmem_inv_shm_buf(phys_addr_t paddr, size_t size);
''')

# ---- pool struct ----
edit(C, '''	size_t max_size;
	spinlock_t lock;
};''', '''	size_t max_size;
	spinlock_t lock;
	bool is_cached;
};''')

# ---- generic mode stubs for the new API ----
edit(C, '''static void qcom_tzmem_cleanup_area(struct qcom_tzmem_area *area)
{

}

#elif''', '''static void qcom_tzmem_cleanup_area(struct qcom_tzmem_area *area)
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

#elif''')

# ---- shmbridge mode: registry + vmid-aware create ----
edit(C, '''#define QCOM_SHM_BRIDGE_NUM_VM_SHIFT 9

static bool qcom_tzmem_using_shm_bridge;
''', '''#define QCOM_SHM_BRIDGE_NUM_VM_SHIFT 9
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
''')

edit(C, '''int qcom_tzmem_shm_bridge_create(phys_addr_t paddr, size_t size, u64 *handle)
{
	u64 pfn_and_ns_perm, ipfn_and_s_perm, size_and_flags;
	int ret;

	if (!qcom_tzmem_using_shm_bridge)
		return 0;

	pfn_and_ns_perm = paddr | QCOM_SCM_PERM_RW;
	ipfn_and_s_perm = paddr | QCOM_SCM_PERM_RW;
	size_and_flags = size | (1 << QCOM_SHM_BRIDGE_NUM_VM_SHIFT);

	ret = qcom_scm_shm_bridge_create(pfn_and_ns_perm, ipfn_and_s_perm,
					 size_and_flags, QCOM_SCM_VMID_HLOS,
					 handle);
	if (ret) {
		dev_err(qcom_tzmem_dev,
			"SHM Bridge failed: ret %d paddr 0x%pa, size %zu\\n",
			ret, &paddr, size);

		return ret;
	}

	return 0;
}
EXPORT_SYMBOL_GPL(qcom_tzmem_shm_bridge_create);''', '''int qcom_tzmem_shm_bridge_create_with_vmid(phys_addr_t paddr, size_t size,
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
			"SHM Bridge failed: ret %d paddr 0x%pa, size %zu vmid %#x\\n",
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
EXPORT_SYMBOL_GPL(qcom_tzmem_shm_bridge_create);''')

edit(C, '''void qcom_tzmem_shm_bridge_delete(u64 handle)
{
	if (qcom_tzmem_using_shm_bridge)
		qcom_scm_shm_bridge_delete(handle);
}''', '''void qcom_tzmem_shm_bridge_delete(u64 handle)
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
	pr_err("qcom_tzmem: unknown SHM bridge handle %#llx\\n", handle);
}''')

# ---- cached pools ----
edit(C, '''	area->size = PAGE_ALIGN(size);

	area->vaddr = dma_alloc_coherent(qcom_tzmem_dev, area->size,
					 &area->paddr, gfp);
	if (!area->vaddr)
		return -ENOMEM;

	ret = qcom_tzmem_init_area(area);
	if (ret) {
		dma_free_coherent(qcom_tzmem_dev, area->size,
				  area->vaddr, area->paddr);
		return ret;
	}

	ret = gen_pool_add_virt(pool->genpool, (unsigned long)area->vaddr,
				(phys_addr_t)area->paddr, size, -1);
	if (ret) {
		dma_free_coherent(qcom_tzmem_dev, area->size,
				  area->vaddr, area->paddr);
		return ret;
	}''', '''	area->size = PAGE_ALIGN(size);

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
	}''')
edit(C, '''static int qcom_tzmem_pool_add_memory(struct qcom_tzmem_pool *pool,
				      size_t size, gfp_t gfp)
{''', '''static void qcom_tzmem_free_area_mem(struct qcom_tzmem_pool *pool,
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
{''')
edit(C, '''	pool->max_size = config->max_size;
	INIT_LIST_HEAD(&pool->areas);''', '''	pool->max_size = config->max_size;
	pool->is_cached = config->is_cached;
	INIT_LIST_HEAD(&pool->areas);''')

s = (SRC / C).read_text()
old = '''		qcom_tzmem_cleanup_area(area);
		dma_free_coherent(qcom_tzmem_dev, area->size,
				  area->vaddr, area->paddr);
		kfree(area);'''
if old in s:
    s = s.replace(old, '''		qcom_tzmem_cleanup_area(area);
		qcom_tzmem_free_area_mem(pool, area);
		kfree(area);''', 1)
    (SRC / C).write_text(s)
elif 'qcom_tzmem_free_area_mem(pool, area);\n\t\tkfree(area);' not in s:
    raise SystemExit('pool_free anchor missing')

edit(C, '''EXPORT_SYMBOL_GPL(qcom_tzmem_to_phys);
''', '''EXPORT_SYMBOL_GPL(qcom_tzmem_to_phys);

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
''')
s = (SRC / C).read_text()
if '#include <linux/qtee_shmbridge.h>' not in s:
    s = s.replace('#include <linux/firmware/qcom/qcom_tzmem.h>\n',
                  '#include <linux/firmware/qcom/qcom_tzmem.h>\n#include <linux/mutex.h>\n#include <linux/qtee_shmbridge.h>\n#include <linux/sizes.h>\n', 1)
    (SRC / C).write_text(s)
print('applied qtee_shmbridge/tzmem parity to', SRC)
