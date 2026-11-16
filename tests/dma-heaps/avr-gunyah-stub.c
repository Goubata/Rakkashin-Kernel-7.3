// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU-only stand-in for the Gunyah resource-manager symbols gh_rm_drv imports.
 * QEMU virt is not a Gunyah guest, so the real gunyah.ko correctly refuses to load;
 * this stub exports the same functions with the same prototypes (CRC-checked) and
 * reports "no hypervisor", which is what mem_buf / qcom_dma_heaps must tolerate.
 * Never shipped: the phone uses ports/gunyah-core.
 */
#include <linux/module.h>
#include <linux/notifier.h>
#include "../../ports/gunyah-core/rsc_mgr.h"
#include <linux/gunyah_qtvm.h>
/* Signatures (incl. parameter names: CRCs come from DWARF) match ports/gunyah-core. */

int gunyah_rm_notifier_register(struct gunyah_rm *rm, struct notifier_block *nb)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(gunyah_rm_notifier_register);

int gunyah_rm_notifier_unregister(struct gunyah_rm *rm, struct notifier_block *nb)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(gunyah_rm_notifier_unregister);

int gunyah_rm_call(struct gunyah_rm *rm, u32 message_id, const void *req_buf,
		   size_t req_buf_size, void **resp_buf, size_t *resp_buf_size)
{
	return -ENODEV;
}
EXPORT_SYMBOL_GPL(gunyah_rm_call);

int gunyah_qtvm_register_notifier(struct notifier_block *nb)
{
	return 0;	/* no QTVM will ever announce itself */
}
EXPORT_SYMBOL_GPL(gunyah_qtvm_register_notifier);

int gunyah_qtvm_unregister_notifier(struct notifier_block *nb)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gunyah_qtvm_unregister_notifier);

/*
 * gh_rm_heap_manager refuses to load without hypervisor heap resources; these are
 * its exports with the behaviour it has when rm_heap_manager_enabled is false.
 */
uint64_t gh_rm_heap_get_hyp_heap_free(void);
uint64_t gh_rm_heap_get_total_heap_added(void);
void gh_rm_heap_memlend_prealloc(size_t dmabuf_size);
void gh_rm_heap_shrink(void);

uint64_t gh_rm_heap_get_hyp_heap_free(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gh_rm_heap_get_hyp_heap_free);

uint64_t gh_rm_heap_get_total_heap_added(void)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gh_rm_heap_get_total_heap_added);

void gh_rm_heap_memlend_prealloc(size_t dmabuf_size)
{
}
EXPORT_SYMBOL_GPL(gh_rm_heap_memlend_prealloc);

void gh_rm_heap_shrink(void)
{
}
EXPORT_SYMBOL_GPL(gh_rm_heap_shrink);

MODULE_LICENSE("GPL");
