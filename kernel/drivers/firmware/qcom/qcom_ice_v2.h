/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef QCOM_ICE_V2_DESC_H
#define QCOM_ICE_V2_DESC_H
#include <linux/arm-smccc.h>
#include "qcom_scm.h"

/* Pinned Motorola qcom_scm.c: ES V2 set/clear/derive are 0x05/0x06/0x07. */
static inline struct qcom_scm_desc qcom_ice_v2_set_desc(u32 slot,
		phys_addr_t key, size_t size, u32 cipher, u32 unit, u32 ce)
{
	return (struct qcom_scm_desc) {
		.svc = QCOM_SCM_SVC_ES, .cmd = 0x05,
		.owner = ARM_SMCCC_OWNER_SIP,
		.arginfo = QCOM_SCM_ARGS(6, QCOM_SCM_VAL, QCOM_SCM_RW,
			QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL),
		.args = { slot, key, size, cipher, unit, ce },
	};
}
static inline struct qcom_scm_desc qcom_ice_v2_clear_desc(u32 slot, u32 ce)
{
	return (struct qcom_scm_desc) {
		.svc = QCOM_SCM_SVC_ES, .cmd = 0x06,
		.owner = ARM_SMCCC_OWNER_SIP, .arginfo = QCOM_SCM_ARGS(2),
		.args = { slot, ce },
	};
}
static inline struct qcom_scm_desc qcom_ice_v2_derive_desc(phys_addr_t key,
		size_t key_size, phys_addr_t secret, size_t secret_size)
{
	return (struct qcom_scm_desc) {
		.svc = QCOM_SCM_SVC_ES, .cmd = 0x07,
		.owner = ARM_SMCCC_OWNER_SIP,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL,
			QCOM_SCM_RW, QCOM_SCM_VAL),
		.args = { key, key_size, secret, secret_size },
	};
}
#endif
