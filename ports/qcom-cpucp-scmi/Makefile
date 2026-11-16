# SPDX-License-Identifier: GPL-2.0-only
# CPU DCVS transport on canoe (canoe_perf.bzl): CPUCP mailbox (QCOM_CPUCP=m),
# Qualcomm SCMI vendor protocol (QTI_SCMI_VENDOR_PROTOCOL=m), SCMI client (QTI_QCOM_SCMI_CLIENT=m), cpucp_fast,
# cpucp_log, cpu_phys_log_map, qcom-cpufreq-hw (ARM_QCOM_CPUFREQ_HW=m).
obj-m += qcom_cpucp.o qcom_scmi_vendor.o qcom_scmi_client.o cpucp_fast.o \
	cpucp_log.o cpu_phys_log_map.o qcom-cpufreq-hw.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../qcom-ipc-logging/include \
	-I$(src)/../sched-walt/include
ccflags-y += -DCONFIG_QTI_SCMI_VENDOR_PROTOCOL_MODULE=1 -DCONFIG_QCOM_CPUCP_MODULE=1 \
	-DCONFIG_QCOM_CPU_PHYS_LOG_MAP_MODULE=1 -DCONFIG_SCHED_WALT_MODULE=1 \
	-DCONFIG_SCHED_WALT_NR_CPUS=8 -DCONFIG_ARM_QCOM_CPUFREQ_HW_MODULE=1 -DCONFIG_QTI_QCOM_SCMI_CLIENT_MODULE=1
ccflags-y += -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../sched-walt/Module.symvers
# clk-scmi: the vendor copy is an older upstream clk-scmi than GKI 6.12; the 7.3 in-tree
# driver (CONFIG_COMMON_CLK_SCMI=m, configs/avr-platform.fragment) replaces it.
