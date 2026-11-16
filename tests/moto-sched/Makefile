# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-moto-sched-test.o
ccflags-y += -I$(src)/../../ports/moto-sched -I$(src)/../../ports/sched-walt/include -I$(src)/../../ports/sched-walt -I$(srctree) -DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8 -DCONFIG_SCHED_MOTO_UNFAIR_MODULE=1 -DCONFIG_MOTO_MUTEX_INHERIT=1 -DCONFIG_MOTO_RWSEM_INHERIT=1 -DCONFIG_MOTO_ENABLE_MDPF=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/sched-walt/Module.symvers $(src)/../../ports/moto-sched/Module.symvers
