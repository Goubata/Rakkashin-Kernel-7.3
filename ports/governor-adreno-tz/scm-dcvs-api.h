/* SPDX-License-Identifier: GPL-2.0-only */
#include <linux/types.h>
extern int qcom_scm_io_reset(void);
extern bool qcom_scm_dcvs_core_available(void);
extern bool qcom_scm_dcvs_ca_available(void);
extern int qcom_scm_dcvs_reset(void);
extern int qcom_scm_dcvs_init_v2(phys_addr_t addr, size_t size, int *version);
extern int qcom_scm_dcvs_init_ca_v2(phys_addr_t addr, size_t size);
extern int qcom_scm_dcvs_update(int level, s64 total_time, s64 busy_time);
extern int qcom_scm_dcvs_update_v2(int level, s64 total_time, s64 busy_time);
extern int qcom_scm_dcvs_update_ca_v2(int level, s64 total_time, s64 busy_time,
				      int context_count);
