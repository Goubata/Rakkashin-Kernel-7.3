/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef QCOM_PAS_RETRY_H
#define QCOM_PAS_RETRY_H
#include <linux/types.h>

/* Shared retry policy; callbacks are the real SCM transport/msleep in use.
 * A disabled retry must preserve the first failure, never report success.
 */
static inline int qcom_pas_retry(u32 id, unsigned int delay_ms,
		int (*shutdown)(u32), void (*delay)(unsigned int))
{
	int ret = shutdown(id);

	if (!ret || !delay_ms)
		return ret;
	delay(delay_ms);
	return shutdown(id);
}
#endif
