# SPDX-License-Identifier: GPL-2.0-only
obj-m += bcl_pmic5.o
bcl_pmic5-y := qti_bcl_pmic5.o qti_bcl_stats.o
ccflags-y += -I$(srctree) -I$(src)/../qcom-ipc-logging/include -DCONFIG_IPC_LOGGING_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers
