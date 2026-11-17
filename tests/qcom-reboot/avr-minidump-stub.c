// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU-only: msm_minidump_enabled() with the ports/qcom-minidump prototype (same
 * CRC). The real minidump needs the bootloader's SMEM minidump table; on the
 * phone minidump.ko is enabled (cmdline qcom_dload_mode.name=mini). Never shipped.
 */
#include <linux/module.h>
#include <soc/qcom/minidump.h>

bool msm_minidump_enabled(void)
{
	return true;
}
EXPORT_SYMBOL_GPL(msm_minidump_enabled);

/* qcom_ipc_logging registers its log buffers; no SMEM table exists on QEMU */
int msm_minidump_add_region(const struct md_region *entry)
{
	return 0;
}
EXPORT_SYMBOL_GPL(msm_minidump_add_region);

int msm_minidump_remove_region(const struct md_region *entry)
{
	return 0;
}
EXPORT_SYMBOL_GPL(msm_minidump_remove_region);
MODULE_LICENSE("GPL");
