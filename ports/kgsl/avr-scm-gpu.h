/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
int qcom_scm_kgsl_dcvs_tuning(u32 mingap, u32 penalty, u32 numbusy);
int qcom_scm_pas_shutdown_retry(u32 peripheral);
int qcom_scm_kgsl_set_smmu_lpac_aperture(unsigned int num_context_bank);
