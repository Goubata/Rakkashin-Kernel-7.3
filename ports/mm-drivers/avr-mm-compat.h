/* SPDX-License-Identifier: GPL-2.0-only */
/* AVR 7.3 port: vendor <linux/remoteproc/qcom_rproc.h> API absent from the
 * android18-7.3 header. canoe has no CONFIG_QCOM_Q6V5_PAS_SOCCP_V1, so the
 * vendor header resolves rproc_set_state() to this inline stub (the phone's
 * msm_hfi_core imports nothing from qcom_q6v5_pas). */
#ifndef AVR_MM_COMPAT_H
#define AVR_MM_COMPAT_H
#include <linux/types.h>
struct rproc;
static inline int rproc_set_state(struct rproc *rproc, bool state)
{
	return 0;
}
#endif
