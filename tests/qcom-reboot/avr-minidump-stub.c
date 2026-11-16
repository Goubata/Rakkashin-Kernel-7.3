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
MODULE_LICENSE("GPL");
