/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef QCOM_SCM_RETRY_POLICY_H
#define QCOM_SCM_RETRY_POLICY_H
#include <linux/types.h>
#include "qcom_scm.h"
/* Match the established busy retry boundary; V2 ICE explicitly disables it. */
static inline bool qcom_scm_retry_busy(bool enabled, long status,
				     unsigned int retries, unsigned int limit)
{
	return enabled && status == QCOM_SCM_V2_EBUSY && retries <= limit;
}
#endif
