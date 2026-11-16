# SPDX-License-Identifier: GPL-2.0-only
obj-m += thermal_pause.o
ccflags-y += -I$(src)/include -I$(src)/../sched-walt/include -DCONFIG_QTI_CPU_PAUSE_COOLING_DEVICE_MODULE=1 -DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8
KBUILD_EXTRA_SYMBOLS += $(src)/../sched-walt/Module.symvers
