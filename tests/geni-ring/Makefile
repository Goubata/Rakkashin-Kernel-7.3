# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-geni-ring-test.o
ccflags-y += -I$(src)/../../ports/geni-bus/include -I$(srctree) -I$(src)/../../ports/qcom-ipc-logging/include -DCONFIG_IPC_LOGGING_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-ipc-logging/Module.symvers $(src)/../../ports/qcom-smem/Module.symvers
