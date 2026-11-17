# SPDX-License-Identifier: GPL-2.0-only
# Debug/diagnostic first-stage modules (canoe_perf.bzl =m): memory_dump_v2,
# qcom_va_minidump, qcom_logbuf_boot_log, qcom_logbuf_vendor_hooks,
# kryo_arm64_edac (PANIC_ON_UE=y, PANIC_ON_CE=n), qcom_iommu_debug.
obj-m += memory_dump_v2.o qcom_va_minidump.o qcom_logbuf_boot_log.o \
	qcom_logbuf_vendor_hooks.o kryo_arm64_edac.o qcom_iommu_debug.o
qcom_iommu_debug-y := qcom-iommu-debug.o qcom-iommu-debug-user.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../qcom-minidump/include -I$(src)/../qcom-minidump \
	-I$(src)/../qcom-iommu-util/include -I$(src)/../secure-buffer/include
ccflags-y += -DCONFIG_QCOM_MEMORY_DUMP_V2_MODULE=1 -DCONFIG_QCOM_DEBUG_SYMBOL_MODULE=1 \
	-DCONFIG_QCOM_MINIDUMP_MODULE=1 -DCONFIG_QCOM_VA_MINIDUMP_MODULE=1 \
	-DCONFIG_EDAC_KRYO_ARM64_PANIC_ON_UE=1 -DCONFIG_QCOM_IOMMU_UTIL_MODULE=1 \
	-DCONFIG_QCOM_LOGBUF_VENDOR_HOOKS_MODULE=1 -DCONFIG_QCOM_LOGBUF_BOOTLOG_MODULE=1
ccflags-y += -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-iommu-util/Module.symvers \
	$(src)/../secure-buffer/Module.symvers
# debug_symbol: provided natively by 7.3 kernel/kallsyms.c (core commit 3684f35f1), not a module.
