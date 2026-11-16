#!/usr/bin/env python3
"""Restore the android16-6.12 vendor SMCInvoke SCM entry points on 7.3.

si_core_module (qcom-soc si_core/qcom_scm_invoke.c) and smcinvoke_dlkm call
qcom_scm_invoke_smc(), qcom_scm_invoke_smc_legacy() and
qcom_scm_invoke_callback_response() with the 6.12 signature (int32 result, the
response type and the extra result[2] "data" word). android18-7.3 only has the
upstream qcomtee variants (u64 result, no data, busy-retrying qcom_scm_call).
6.12 issues these with QCOM_SCM_CALL_NORETRY: si_core implements its own
-EBUSY back-off (SCM_EBUSY_WAIT_MS x SCM_EBUSY_MAX_RETRY) and must see the raw
busy result, so these use the existing 7.3 __scm_smc_call_noretry() path.
6.12's multi-SMC concurrency (qcom_scm_sem_lock, whitelisted multicall) is not
part of this patch; 7.3 serialises standard SCM calls like 6.12 does without it.
FF-A and doorbell variants are not built for canoe (no CONFIG_QCOM_SI_CORE_MEM_FFA
or _DOORBELL in canoe_perf.bzl) and are not restored.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
C = 'drivers/firmware/qcom/qcom_scm.c'
H = 'include/linux/firmware/qcom/qcom_scm.h'


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:70]!r}')
    p.write_text(s.replace(old, new, 1))


edit(H, '''int qcom_scm_qtee_invoke_smc(phys_addr_t inbuf, size_t inbuf_size,''', '''/* ANDROID: vendor SMCInvoke entry points (android16-6.12 signatures) */
int qcom_scm_invoke_smc(phys_addr_t in_buf, size_t in_buf_size,
			phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
			u64 *response_type, unsigned int *data);
int qcom_scm_invoke_smc_legacy(phys_addr_t in_buf, size_t in_buf_size,
			       phys_addr_t out_buf, size_t out_buf_size,
			       int32_t *result, u64 *response_type,
			       unsigned int *data);
int qcom_scm_invoke_callback_response(phys_addr_t out_buf, size_t out_buf_size,
				      int32_t *result, u64 *response_type,
				      unsigned int *data);

int qcom_scm_qtee_invoke_smc(phys_addr_t inbuf, size_t inbuf_size,''')

edit(C, '''/**
 * qcom_scm_qtee_invoke_smc() - Invoke a QTEE object.''', '''/*
 * ANDROID: 6.12 vendor SMCInvoke calls. No -EBUSY replay (the caller backs off),
 * results are reported even on error exactly as 6.12 did.
 */
static int qcom_scm_vendor_invoke(struct qcom_scm_desc *desc, int32_t *result,
				  u64 *response_type, unsigned int *data)
{
	enum qcom_scm_convention convention = __get_convention();
	struct qcom_scm_res res = {};
	int ret;

	if (!__scm)
		return -EPROBE_DEFER;
	if (convention != SMC_CONVENTION_ARM_32 &&
	    convention != SMC_CONVENTION_ARM_64)
		return -EOPNOTSUPP;

	might_sleep();
	ret = __scm_smc_call_noretry(__scm->dev, desc, convention, &res);
	if (result)
		*result = res.result[1];
	if (response_type)
		*response_type = res.result[0];
	if (data)
		*data = res.result[2];
	return ret;
}

int qcom_scm_invoke_smc(phys_addr_t in_buf, size_t in_buf_size,
			phys_addr_t out_buf, size_t out_buf_size, int32_t *result,
			u64 *response_type, unsigned int *data)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_INVOKE,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = in_buf,
		.args[1] = in_buf_size,
		.args[2] = out_buf,
		.args[3] = out_buf_size,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL,
					 QCOM_SCM_RW, QCOM_SCM_VAL),
	};

	return qcom_scm_vendor_invoke(&desc, result, response_type, data);
}
EXPORT_SYMBOL(qcom_scm_invoke_smc);

int qcom_scm_invoke_smc_legacy(phys_addr_t in_buf, size_t in_buf_size,
			       phys_addr_t out_buf, size_t out_buf_size,
			       int32_t *result, u64 *response_type,
			       unsigned int *data)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_INVOKE_LEGACY,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = in_buf,
		.args[1] = in_buf_size,
		.args[2] = out_buf,
		.args[3] = out_buf_size,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL,
					 QCOM_SCM_RW, QCOM_SCM_VAL),
	};

	return qcom_scm_vendor_invoke(&desc, result, response_type, data);
}
EXPORT_SYMBOL(qcom_scm_invoke_smc_legacy);

int qcom_scm_invoke_callback_response(phys_addr_t out_buf, size_t out_buf_size,
				      int32_t *result, u64 *response_type,
				      unsigned int *data)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_CB_RSP,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = out_buf,
		.args[1] = out_buf_size,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL),
	};

	return qcom_scm_vendor_invoke(&desc, result, response_type, data);
}
EXPORT_SYMBOL(qcom_scm_invoke_callback_response);

/**
 * qcom_scm_qtee_invoke_smc() - Invoke a QTEE object.''')
print('applied vendor SMCInvoke SCM calls to', SRC)
