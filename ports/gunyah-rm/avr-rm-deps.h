/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_RM_DEPS_H
#define AVR_RM_DEPS_H
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/gunyah/gh_rm_drv.h>
/* Exact vendor declarations. These do not supply a backend or suppress modpost. */
#ifndef QCOM_SCM_VMID_SOCCP
#define QCOM_SCM_VMID_SOCCP 0x3c
#endif
struct qcom_scm_assign_mem_notifier_data {
	phys_addr_t mem_addr;
	size_t mem_sz;
	u64 *srcvm;
	const struct qcom_scm_vmperm *newvm;
	unsigned int dest_cnt;
};
int qcom_scm_assign_mem_notifier_register(struct notifier_block *nb);
int qcom_scm_assign_mem_notifier_unregister(struct notifier_block *nb);
int gh_rm_mem_append(gh_memparcel_handle_t handle, u8 flags, struct gh_sgl_entry *sgl_entries, u32 n_sgl_entries);
#endif
