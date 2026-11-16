# SPDX-License-Identifier: GPL-2.0-only
# canoe (moto-canoe.config): MOTO_MUTEX_INHERIT=y, MOTO_RWSEM_INHERIT=y, MOTO_ENABLE_MDPF=y;
# FUTEX_INHERIT and LOCKING_2 off. Depends on sched_walt (MOTO_UNFAIR build).
obj-m += moto_sched.o
moto_sched-y := msched_main.o msched_common.o msched_sysfs.o msched_oemdata.o \
	locking/locking_main.o locking/locking_trace.o locking/mutex.o locking/rwsem.o \
	msched_trace.o msched_uclamp.o mdpf/mdpf_sysfs.o
ccflags-y += -I$(src) -I$(src)/../sched-walt/include -I$(src)/../sched-walt -I$(srctree) \
	-DCONFIG_MOTO_MUTEX_INHERIT=1 -DCONFIG_MOTO_RWSEM_INHERIT=1 -DCONFIG_MOTO_ENABLE_MDPF=1 \
	-DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8 -DCONFIG_SCHED_MOTO_UNFAIR_MODULE=1 \
	-Wno-missing-prototypes
CFLAGS_msched_trace.o := -I$(src)
CFLAGS_locking/locking_trace.o := -I$(src)/locking
KBUILD_EXTRA_SYMBOLS += $(src)/../sched-walt/Module.symvers
