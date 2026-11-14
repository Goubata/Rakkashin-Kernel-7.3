# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-diagnostics-test.o
ccflags-y += -I$(src)/../../ports/qcom-minidump/include -I$(src)/../../ports/qcom-ipc-logging/include
ccflags-y += -DCONFIG_QCOM_MINIDUMP_MODULE=1 -DCONFIG_IPC_LOGGING_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-minidump/Module.symvers $(src)/../../ports/qcom-ipc-logging/Module.symvers
