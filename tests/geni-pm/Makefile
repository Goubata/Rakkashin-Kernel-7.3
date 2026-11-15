# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-geni-pm-test.o
ccflags-y += -I$(src)/../../ports/geni-bus/include -I$(src)/../../ports/qcom-ipc-logging/include -DCONFIG_IPC_LOGGING_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-ipc-logging/Module.symvers
