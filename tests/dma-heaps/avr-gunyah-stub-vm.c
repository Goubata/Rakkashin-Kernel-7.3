// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU-only: gunyah_vm_get/put for gh_vcpu_mgr (needed by gh_panic_notifier).
 * Same include view as ports/gunyah-core/vm_mgr.c so the exported CRCs (which
 * cover every type reachable from struct gunyah_vm) match. No VM exists on QEMU
 * virt. Never shipped.
 */
#include <linux/anon_inodes.h>
#include <linux/compat.h>
#include <linux/file.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/xarray.h>

#include <uapi/linux/gunyah.h>
#include <trace/hooks/gunyah.h>

#include "../../ports/gunyah-core/rsc_mgr.h"
#include "../../ports/gunyah-core/vm_mgr.h"

int __must_check gunyah_vm_get(struct gunyah_vm *ghvm)
{
	return 0;
}
EXPORT_SYMBOL_GPL(gunyah_vm_get);

void gunyah_vm_put(struct gunyah_vm *ghvm)
{
}
EXPORT_SYMBOL_GPL(gunyah_vm_put);
