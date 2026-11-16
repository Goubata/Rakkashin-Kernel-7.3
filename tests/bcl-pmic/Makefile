# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-bcl-pmic-test.o avr-bcl-pmic-probe-test.o
ccflags-y += -I$(srctree) -I$(src)/../../ports/qcom-ipc-logging/include -DCONFIG_IPC_LOGGING_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-ipc-logging/Module.symvers
