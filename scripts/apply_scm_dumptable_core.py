#!/usr/bin/env python3
"""Restore qcom_scm_assign_dump_table_region() (android16-6.12, qcom_scm-scm.c).

memory_dump_v2.ko registers the crash-dump table with TZ through this call
(SIP svc UTIL 0x03, cmd DUMP_TABLE_ASSIGN 0x13, args: assign, addr, size).
canoe does not set CONFIG_QCOM_SCM_SMCI, so 6.12 used this plain SCM variant.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')


def edit(rel, old, new):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != 1:
        raise SystemExit(f'{rel}: anchor count {s.count(old)}')
    p.write_text(s.replace(old, new, 1))


edit('include/linux/firmware/qcom/qcom_scm.h', 'int qcom_scm_disable_sdi(void);\n',
     'int qcom_scm_disable_sdi(void);\nint qcom_scm_assign_dump_table_region(bool is_assign, phys_addr_t addr, size_t size);\n')
edit('drivers/firmware/qcom/qcom_scm.h', '#define QCOM_SCM_SVC_OEM_POWER			0x09\n',
     '#define QCOM_SCM_SVC_OEM_POWER			0x09\n/* ANDROID: crash dump table registration (6.12 qcom_scm-scm.c) */\n#define QCOM_SCM_SVC_UTIL			0x03\n#define QCOM_SCM_UTIL_DUMP_TABLE_ASSIGN		0x13\n')
edit('drivers/firmware/qcom/qcom_scm.c', 'EXPORT_SYMBOL_GPL(qcom_scm_get_download_mode);\n', '''EXPORT_SYMBOL_GPL(qcom_scm_get_download_mode);

/* ANDROID: 6.12 qcom_scm-scm.c: hand the memory_dump_v2 table to TZ */
int qcom_scm_assign_dump_table_region(bool is_assign, phys_addr_t addr, size_t size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_UTIL,
		.cmd = QCOM_SCM_UTIL_DUMP_TABLE_ASSIGN,
		.arginfo = QCOM_SCM_ARGS(3),
		.owner = ARM_SMCCC_OWNER_SIP,
		.args[0] = is_assign,
		.args[1] = addr,
		.args[2] = size,
	};

	if (!__scm)
		return -ENODEV;

	return qcom_scm_call(__scm->dev, &desc, NULL);
}
EXPORT_SYMBOL_GPL(qcom_scm_assign_dump_table_region);
''')
print('applied dump-table SCM call to', SRC)
