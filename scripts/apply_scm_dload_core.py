#!/usr/bin/env python3
"""Restore android16-6.12 qcom_scm download-mode / restart behaviour on 7.3.

Vendor qcom-dload-mode.ko (first-stage on the phone; cmdline
qcom_dload_mode.name=mini) uses the exported 6.12 API:
  qcom_scm_set_download_mode(enum qcom_download_mode)  (EDL=0x1, FULL=0x10,
      MINI=0x20 written to the TCSR dload register as a whole, like 6.12)
  qcom_scm_get_download_mode(unsigned int *)
  qcom_scm_disable_sdi()
and binds to the "qcom-dload-mode" platform device that 6.12 qcom_scm_probe()
registers when the DT has qcom,dload-mode and no /soc/dload_mode node.
android18-7.3 keeps these static (different signature), always rewrites the
dump field at probe and shutdown, and disables SDI at probe unless download
mode is on. 6.12 only touched download mode when the qcom_scm.download_mode
parameter was set and disabled SDI at probe only for qcom,sdi-enabled; its
shutdown always disabled SDI. 6.12 also registered a restart handler
(priority 130) issuing the OEM warm reboot SCM call for REBOOT_WARM and the
rtc/twm custom shutdown-reboots. All of that is restored here.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
C = 'drivers/firmware/qcom/qcom_scm.c'
PH = 'drivers/firmware/qcom/qcom_scm.h'
H = 'include/linux/firmware/qcom/qcom_scm.h'


def edit(rel, old, new, count=1):
    p = SRC / rel
    s = p.read_text()
    if new in s:
        return
    if s.count(old) != count:
        raise SystemExit(f'{rel}: anchor count {s.count(old)} != {count}: {old[:70]!r}')
    p.write_text(s.replace(old, new))


edit(H, 'int qcom_scm_invoke_smc(phys_addr_t in_buf, size_t in_buf_size,', '''/* ANDROID: vendor download mode / SDI / restart API (android16-6.12) */
enum qcom_download_mode {
	QCOM_DOWNLOAD_NODUMP	= 0x00,
	QCOM_DOWNLOAD_EDL	= 0x01,
	QCOM_DOWNLOAD_FULLDUMP	= 0x10,
	QCOM_DOWNLOAD_MINIDUMP	= 0x20,
};

enum qcom_scm_custom_reset_type {
	QCOM_SCM_RST_NONE,
	QCOM_SCM_RST_SHUTDOWN_TO_RTC_MODE = 0x80000005,
	QCOM_SCM_RST_SHUTDOWN_TO_TWM_MODE,
	QCOM_SCM_RST_MAX
};

int qcom_scm_disable_sdi(void);
void qcom_scm_set_download_mode(enum qcom_download_mode mode);
int qcom_scm_get_download_mode(unsigned int *mode);

int qcom_scm_invoke_smc(phys_addr_t in_buf, size_t in_buf_size,''')

edit(PH, '#define QCOM_SCM_BOOT_SET_DLOAD_MODE	0x10\n', '''#define QCOM_SCM_BOOT_SET_DLOAD_MODE	0x10
/* ANDROID: OEM power service (6.12 warm/custom reboot) */
#define QCOM_SCM_SVC_OEM_POWER			0x09
#define QCOM_SCM_OEM_POWER_REBOOT		0x22
#define QCOM_SCM_OEM_POWER_CUSTOM_REBOOT	0x23
''')

edit(C, '''	struct qcom_tzmem_pool *mempool;
	unsigned int wq_cnt;
};''', '''	struct qcom_tzmem_pool *mempool;
	unsigned int wq_cnt;
	struct notifier_block restart_nb;	/* ANDROID: 6.12 OEM reboot */
};''')

# internal 7.3 field writer keeps its behaviour under a new name
edit(C, 'static void qcom_scm_set_download_mode(struct qcom_scm *scm, u32 dload_mode)',
     'static void qcom_scm_set_dload_field(struct qcom_scm *scm, u32 dload_mode)')
edit(C, '		qcom_scm_set_download_mode(scm, download_mode);\n\n\treturn 0;\n}',
     '		qcom_scm_set_dload_field(scm, download_mode);\n\n\treturn 0;\n}')

edit(C, 'static int qcom_scm_disable_sdi(void)\n', 'int qcom_scm_disable_sdi(void)\n')
edit(C, '''	return ret ? : res.result[0];
}

static int __qcom_scm_set_dload_mode(struct device *dev, bool enable)''', '''	return ret ? : res.result[0];
}
EXPORT_SYMBOL_GPL(qcom_scm_disable_sdi);

static int __qcom_scm_set_dload_mode(struct device *dev, bool enable)''')

edit(C, '''	if (scm->minidump_sram && (dload_mode & QCOM_DLOAD_MINIDUMP))
		writel_relaxed(minidump_dest, scm->minidump_sram);
}
''', '''	if (scm->minidump_sram && (dload_mode & QCOM_DLOAD_MINIDUMP))
		writel_relaxed(minidump_dest, scm->minidump_sram);
}

/* ANDROID: 6.12 vendor API: whole-register write of the vendor mode value */
void qcom_scm_set_download_mode(enum qcom_download_mode mode)
{
	struct device *dev = __scm ? __scm->dev : NULL;
	int ret = 0;

	if (__scm && __scm->dload_mode_addr) {
		ret = qcom_scm_io_writel(__scm->dload_mode_addr, mode);
	} else if (__scm && __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_BOOT,
							 QCOM_SCM_BOOT_SET_DLOAD_MODE)) {
		ret = __qcom_scm_set_dload_mode(dev, mode != QCOM_DOWNLOAD_NODUMP);
	} else {
		dev_err(dev, "No available mechanism for setting download mode\\n");
	}

	if (ret)
		dev_err(dev, "failed to set download mode: %d\\n", ret);
}
EXPORT_SYMBOL_GPL(qcom_scm_set_download_mode);

int qcom_scm_get_download_mode(unsigned int *mode)
{
	struct device *dev = __scm ? __scm->dev : NULL;
	int ret = -EINVAL;

	if (__scm && __scm->dload_mode_addr)
		ret = qcom_scm_io_readl(__scm->dload_mode_addr, mode);
	else
		dev_err(dev, "No available mechanism for getting download mode\\n");

	if (ret)
		dev_err(dev, "failed to get download mode: %d\\n", ret);
	return ret;
}
EXPORT_SYMBOL_GPL(qcom_scm_get_download_mode);

static enum qcom_scm_custom_reset_type qcom_scm_custom_reset_type = QCOM_SCM_RST_NONE;

static int qcom_scm_oem_reboot(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_OEM_POWER,
		.cmd = QCOM_SCM_OEM_POWER_REBOOT,
		.owner = ARM_SMCCC_OWNER_OEM,
	};

	return qcom_scm_call_atomic(dev, &desc, NULL);
}

static int qcom_scm_oem_custom_reboot(struct device *dev,
				      enum qcom_scm_custom_reset_type type)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_OEM_POWER,
		.cmd = QCOM_SCM_OEM_POWER_CUSTOM_REBOOT,
		.owner = ARM_SMCCC_OWNER_OEM,
		.args[0] = type,
		.arginfo = QCOM_SCM_ARGS(1),
	};

	return qcom_scm_call_atomic(dev, &desc, NULL);
}

static int qcom_scm_do_restart(struct notifier_block *nb, unsigned long event,
			       void *ptr)
{
	struct qcom_scm *scm = container_of(nb, struct qcom_scm, restart_nb);
	const char *cmd = ptr;

	if (reboot_mode == REBOOT_WARM &&
	    qcom_scm_custom_reset_type == QCOM_SCM_RST_NONE)
		qcom_scm_oem_reboot(scm->dev);
	else if (cmd && !strcmp(cmd, "rtc"))
		qcom_scm_custom_reset_type = QCOM_SCM_RST_SHUTDOWN_TO_RTC_MODE;
	else if (cmd && !strcmp(cmd, "twm"))
		qcom_scm_custom_reset_type = QCOM_SCM_RST_SHUTDOWN_TO_TWM_MODE;

	if (qcom_scm_custom_reset_type > QCOM_SCM_RST_NONE &&
	    qcom_scm_custom_reset_type < QCOM_SCM_RST_MAX)
		qcom_scm_oem_custom_reboot(scm->dev, qcom_scm_custom_reset_type);

	return NOTIFY_OK;
}
''')

edit(C, '''	/*
	 * If "download mode" is requested, from this point on warmboot
	 * will cause the boot stages to enter download mode, unless
	 * disabled below by a clean shutdown/reboot.
	 */
	qcom_scm_set_download_mode(scm, download_mode);

	/*
	 * Disable SDI if indicated by DT that it is enabled by default.
	 */
	if (of_property_read_bool(pdev->dev.of_node, "qcom,sdi-enabled") || !download_mode)
		qcom_scm_disable_sdi();
''', '''	/* ANDROID: 6.12 restart handler (OEM warm reboot, rtc/twm resets) */
	scm->restart_nb.notifier_call = qcom_scm_do_restart;
	scm->restart_nb.priority = 130;
	register_restart_handler(&scm->restart_nb);

	/* ANDROID: vendor qcom-dload-mode.ko binds to this device (6.12) */
	if (scm->dload_mode_addr) {
		struct device_node *dload = of_find_node_by_path("/soc/dload_mode");

		if (dload)
			of_node_put(dload);
		else if (IS_ERR(platform_device_register_data(&pdev->dev,
				"qcom-dload-mode", PLATFORM_DEVID_NONE, NULL, 0)))
			dev_err(&pdev->dev, "Failed to register qcom dload device\\n");
	}

	/*
	 * If "download mode" is requested, from this point on warmboot
	 * will cause the boot stages to enter download mode, unless
	 * disabled below by a clean shutdown/reboot. ANDROID: as on 6.12 only
	 * when requested; otherwise qcom-dload-mode owns the register.
	 */
	if (download_mode)
		qcom_scm_set_dload_field(scm, download_mode);

	/*
	 * Disable SDI if indicated by DT that it is enabled by default.
	 */
	if (of_property_read_bool(pdev->dev.of_node, "qcom,sdi-enabled"))
		qcom_scm_disable_sdi();
''')

edit(C, '''	/* Clean shutdown, disable download mode to allow normal restart */
	qcom_scm_set_download_mode(__scm, QCOM_DLOAD_NODUMP);''', '''	/* ANDROID: 6.12: always drop SDI, clear dump mode only if we set it */
	qcom_scm_disable_sdi();
	/* Clean shutdown, disable download mode to allow normal restart */
	if (download_mode)
		qcom_scm_set_dload_field(__scm, QCOM_DLOAD_NODUMP);''')

s = (SRC / C).read_text()
if '#include <linux/reboot.h>' not in s:
    s = s.replace('#include <linux/platform_device.h>\n', '#include <linux/platform_device.h>\n#include <linux/reboot.h>\n', 1)
    (SRC / C).write_text(s)
print('applied 6.12 dload/restart SCM behaviour to', SRC)
