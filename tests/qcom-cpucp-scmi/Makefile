# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-fake-scmi.o avr-scmi-vendor-test.o
ccflags-y += -I$(src)/../../ports/qcom-cpucp-scmi/include -DCONFIG_QTI_SCMI_VENDOR_PROTOCOL_MODULE=1 -DCONFIG_QTI_QCOM_SCMI_CLIENT_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-cpucp-scmi/Module.symvers
