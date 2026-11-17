#!/usr/bin/env python3
"""Restore android16-6.12 vendor qcom_scm calls still imported by phone modules.

Found by diffing the phone's qcom_scm exports against the 7.3 Module.symvers and
the undefined symbols of /vendor_dlkm modules (device-baseline copy):
  tz_log_dlkm  : get_tz_feat_id_version, get_tz_log_feat_id,
                 query_encrypted_log_feature, query_log_status, query_tz_time,
                 register_qsee_log_buf, request_encrypted_log  (QSEELOG svc 0x01)
  coresight-tpdm, ipam : get_sec_dump_state (UTIL 0x03/0x10)
  ipam         : mem_protect_region_id (MP 0x0c/0x10)
  msm_drm      : mem_protect_sd_ctrl (MP 0x0c/0x18)
  camera       : smmu_notify_secure_lut (SMMU_PROGRAM 0x15/0x03)
Bodies, SMC ids, owners, arginfo and export kinds are copied from qcom-soc
drivers/firmware/qcom/qcom_scm.c (android16-6.12).
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
C = 'drivers/firmware/qcom/qcom_scm.c'
PH = 'drivers/firmware/qcom/qcom_scm.h'
H = 'include/linux/firmware/qcom/qcom_scm.h'


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}: {old[:60]!r}')
    p.write_text(s.replace(old, new))


edit(PH, '#define QCOM_SCM_INFO_IS_CALL_AVAIL	0x01\n', '''#define QCOM_SCM_INFO_IS_CALL_AVAIL	0x01
/* ANDROID: android16-6.12 vendor SMC ids */
#define QCOM_SCM_INFO_GET_FEAT_VERSION_CMD	0x03
#define QCOM_SCM_FEAT_LOG_ID			0x0a
#define QCOM_SCM_UTIL_GET_SEC_DUMP_STATE	0x10
#define QCOM_SCM_MP_MEM_PROTECT_REGION_ID	0x10
#define QCOM_SCM_MP_CMD_SD_CTRL			0x18
#define QCOM_SCM_SMMU_SECURE_LUT		0x03
#define QCOM_SCM_SVC_QSEELOG			0x01
#define QCOM_SCM_QSEELOG_REGISTER		0x06
#define QCOM_SCM_QUERY_ENCR_LOG_FEAT_ID		0x0b
#define QCOM_SCM_REQUEST_ENCR_LOG_ID		0x0c
#define QCOM_SCM_QUERY_LOG_STATUS		0x0F
#define QCOM_SCM_QUERY_TZ_TIME_ID		0x11
''')

edit(H, 'int qcom_scm_disable_sdi(void);\n', '''int qcom_scm_disable_sdi(void);
/* ANDROID: android16-6.12 vendor calls (tz_log, tpdm, ipam, msm_drm, camera) */
int qcom_scm_get_tz_feat_id_version(u64 feat_id, u64 *version);
int qcom_scm_get_tz_log_feat_id(u64 *version);
int qcom_scm_query_encrypted_log_feature(u64 *enabled);
int qcom_scm_query_log_status(u64 *status);
int qcom_scm_query_tz_time(u64 *ticks, u32 *frequency);
int qcom_scm_register_qsee_log_buf(phys_addr_t buf, size_t len);
int qcom_scm_request_encrypted_log(phys_addr_t buf, size_t len, uint32_t log_id,
				   bool is_full_encrypted_tz_logs_supported,
				   bool is_full_encrypted_tz_logs_enabled);
int qcom_scm_get_sec_dump_state(u32 *dump_state);
int qcom_scm_mem_protect_region_id(phys_addr_t paddr, size_t size);
int qcom_scm_mem_protect_sd_ctrl(u32 devid, phys_addr_t mem_addr, u64 mem_size, u32 vmid);
int qcom_scm_smmu_notify_secure_lut(u64 dev_id, bool secure);
''')

edit(C, 'EXPORT_SYMBOL_GPL(qcom_scm_get_download_mode);\n', '''EXPORT_SYMBOL_GPL(qcom_scm_get_download_mode);

/* ANDROID: android16-6.12 vendor calls */
static int __qcom_scm_get_feat_version(struct device *dev, u64 feat_id, u64 *version)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_GET_FEAT_VERSION_CMD,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = feat_id,
		.arginfo = QCOM_SCM_ARGS(1),
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(dev, &desc, &res);
	if (version)
		*version = res.result[0];
	return ret;
}

int qcom_scm_get_tz_log_feat_id(u64 *version)
{
	return __qcom_scm_get_feat_version(__scm->dev, QCOM_SCM_FEAT_LOG_ID, version);
}
EXPORT_SYMBOL(qcom_scm_get_tz_log_feat_id);

int qcom_scm_get_tz_feat_id_version(u64 feat_id, u64 *version)
{
	return __qcom_scm_get_feat_version(__scm->dev, feat_id, version);
}
EXPORT_SYMBOL(qcom_scm_get_tz_feat_id_version);

static int __qcom_scm_qseelog_query(u32 cmd, struct qcom_scm_res *res)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_QSEELOG,
		.cmd = cmd,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};

	return qcom_scm_call(__scm->dev, &desc, res);
}

int qcom_scm_query_encrypted_log_feature(u64 *enabled)
{
	struct qcom_scm_res res;
	int ret = __qcom_scm_qseelog_query(QCOM_SCM_QUERY_ENCR_LOG_FEAT_ID, &res);

	if (!ret)
		*enabled = res.result[0];
	return ret;
}
EXPORT_SYMBOL(qcom_scm_query_encrypted_log_feature);

int qcom_scm_query_log_status(u64 *status)
{
	struct qcom_scm_res res;
	int ret = __qcom_scm_qseelog_query(QCOM_SCM_QUERY_LOG_STATUS, &res);

	if (!ret)
		*status = res.result[0];
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_query_log_status);

int qcom_scm_query_tz_time(u64 *ticks, u32 *frequency)
{
	struct qcom_scm_res res;
	int ret = __qcom_scm_qseelog_query(QCOM_SCM_QUERY_TZ_TIME_ID, &res);

	if (!ret) {
		*ticks = ((u64)res.result[0] << 32) | (u64)res.result[1];
		*frequency = (u32)res.result[2];
	}
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_query_tz_time);

int qcom_scm_register_qsee_log_buf(phys_addr_t buf, size_t len)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_QSEELOG,
		.cmd = QCOM_SCM_QSEELOG_REGISTER,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = buf,
		.args[1] = len,
		.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW),
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? : res.result[0];
}
EXPORT_SYMBOL(qcom_scm_register_qsee_log_buf);

int qcom_scm_request_encrypted_log(phys_addr_t buf, size_t len, uint32_t log_id,
				   bool is_full_tz_logs_supported,
				   bool is_full_tz_logs_enabled)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_QSEELOG,
		.cmd = QCOM_SCM_REQUEST_ENCR_LOG_ID,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS,
		.args[0] = buf,
		.args[1] = len,
		.args[2] = log_id
	};
	struct qcom_scm_res res;

	if (is_full_tz_logs_supported) {
		/* 1: full logs, 0: incremental logs */
		desc.args[3] = is_full_tz_logs_enabled ? 1 : 0;
		desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW);
	} else {
		desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_RW);
	}
	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? : res.result[0];
}
EXPORT_SYMBOL(qcom_scm_request_encrypted_log);

int qcom_scm_get_sec_dump_state(u32 *dump_state)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_UTIL,
		.cmd = QCOM_SCM_UTIL_GET_SEC_DUMP_STATE,
		.owner = ARM_SMCCC_OWNER_SIP
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(__scm ? __scm->dev : NULL, &desc, &res);
	if (dump_state)
		*dump_state = res.result[0];
	return ret;
}
EXPORT_SYMBOL(qcom_scm_get_sec_dump_state);

int qcom_scm_mem_protect_region_id(phys_addr_t paddr, size_t size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_MEM_PROTECT_REGION_ID,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = paddr,
		.args[1] = size,
		.arginfo = QCOM_SCM_ARGS(2),
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL(qcom_scm_mem_protect_region_id);

int qcom_scm_mem_protect_sd_ctrl(u32 devid, phys_addr_t mem_addr, u64 mem_size, u32 vmid)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_CMD_SD_CTRL,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = devid,
		.args[1] = mem_addr,
		.args[2] = mem_size,
		.args[3] = vmid,
		.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_VAL, QCOM_SCM_RW,
					 QCOM_SCM_VAL, QCOM_SCM_VAL)
	};
	struct qcom_scm_res res;

	ret = qcom_scm_call(__scm->dev, &desc, &res);
	return ret ? : res.result[0];
}
EXPORT_SYMBOL(qcom_scm_mem_protect_sd_ctrl);

int qcom_scm_smmu_notify_secure_lut(u64 dev_id, bool secure)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = QCOM_SCM_SMMU_SECURE_LUT,
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = dev_id,
		.args[1] = secure,
		.arginfo = QCOM_SCM_ARGS(2),
	};

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL(qcom_scm_smmu_notify_secure_lut);
''')
print('applied 6.12 vendor qcom_scm calls to', SRC)
