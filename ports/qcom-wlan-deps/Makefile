# SPDX-License-Identifier: GPL-2.0-only
# smem-mailbox.ko: vendor/qcom/opensource/data-kernel drivers/smem-mailbox (tag
#   MMI-W3WWL36.65-11-18), imported by cnss_utils (CONFIG_FEATURE_SMEM_MAILBOX).
# qcom_ramdump.ko: qcom-soc drivers/soc/qcom/qcom_ramdump.c (canoe_perf =m), imported by
#   icnss2, qcom_q6v5_pas, ipam, microdump_collector.
obj-m += smem-mailbox.o qcom_ramdump.o
ccflags-y += -I$(src)/include -I$(src)/../qcom-smem/include -I$(src)/../qcom-minidump/include \
	-Wno-missing-prototypes -DCONFIG_QCOM_RAMDUMP_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers $(src)/../qcom-minidump/Module.symvers \
	$(src)/../qcom-smp2p/Module.symvers
