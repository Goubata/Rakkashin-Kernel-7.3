# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-perf-test.o
ccflags-y += -I$(src)/../../ports/qcom-perf-misc/include -DCONFIG_QCOM_PMU_LIB_MODULE=1 -DCONFIG_QTI_SCMI_VENDOR_PROTOCOL_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-perf-misc/Module.symvers
