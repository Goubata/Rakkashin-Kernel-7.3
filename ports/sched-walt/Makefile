# SPDX-License-Identifier: GPL-2.0-only
# canoe_perf: SCHED_WALT=m, STATS=n, DEBUG=n, MIDPOINT (phone exposes midpoint_freq), MOTO_UNFAIR=m
obj-m += sched-walt.o
sched-walt-y := walt.o boost.o sched_avg.o walt_halt.o core_ctl.o trace.o input-boost.o \
	sysctl.o cpufreq_walt.o fixup.o walt_lb.o walt_rt.o walt_cfs.o walt_tp.o walt_config.o \
	voter.o walt_cpufreq_cycle_cntr_driver.o walt_gclk_cycle_counter_driver.o walt_cycles.o \
	debugfs.o pipeline.o smart_freq.o walt_storage_lb.o mvp_locking.o midpoint.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../qcom-socinfo/include \
	-DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_MIDPOINT_MODULE=1 \
	-DCONFIG_SCHED_MOTO_UNFAIR_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8 \
	-Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-smem/Module.symvers
CFLAGS_trace.o := -I$(src)
CFLAGS_walt.o := -I$(src)
