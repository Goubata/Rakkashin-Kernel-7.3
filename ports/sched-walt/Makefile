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
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
