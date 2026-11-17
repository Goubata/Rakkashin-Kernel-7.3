# SPDX-License-Identifier: GPL-2.0-only
# canoe power management (canoe_perf.bzl =m): qcom_lpm cpuidle governor
# (CPU_IDLE_GOV_QCOM_LPM), qcom_lpm_monitor, qcom_stats, sys_pm_vx,
# qcom_cpuss_sleep_stats(_v4), smp2p_sleepstate, msm_show_resume_irq,
# boot_stats, msm_show_epoch, sysmon_subsystem_stats.
obj-m += qcom_lpm.o qcom_lpm_monitor.o qcom_stats.o sys_pm_vx.o qcom_cpuss_sleep_stats.o \
	qcom_cpuss_sleep_stats_v4.o smp2p_sleepstate.o msm_show_resume_irq.o boot_stats.o \
	msm_show_epoch.o sysmon_subsystem_stats.o
qcom_lpm-y := qcom-cluster-lpm.o qcom-lpm-sysfs.o qcom-lpm.o
qcom_lpm_monitor-y := qcom_lpm_monitor_service_v01.o qcom_lpm_monitor_main.o
ccflags-y += -I$(src)/include -I$(src)/include/uapi -I$(src) -I$(srctree) -I$(src)/../qcom-smem/include \
	-I$(src)/../sched-walt/include -I$(src)/../qcom-remoteproc-services/include
ccflags-y += -DCONFIG_CPU_IDLE_GOV_QCOM_LPM_MODULE=1 -DCONFIG_SCHED_WALT_MODULE=1 \
	-DCONFIG_SCHED_WALT_NR_CPUS=8 -DCONFIG_QCOM_STATS_MODULE=1 -DCONFIG_QCOM_LPM_MONITOR_MODULE=1 \
	-DCONFIG_QCOM_SYSMON_SUBSYSTEM_STATS_MODULE=1 \
	-DCONFIG_SHOW_SUSPEND_EPOCH_MODULE=1 -DCONFIG_QCOM_SHOW_RESUME_IRQ_MODULE=1 -DCONFIG_MSM_BOOT_STATS_MODULE=1 \
	-DCONFIG_QCOM_SMP2P_SLEEPSTATE_MODULE=1 -DCONFIG_QTI_SYS_PM_VX_MODULE=1 -DCONFIG_QCOM_CPUSS_SLEEP_STATS_MODULE=1 \
	-DCONFIG_QCOM_CPUSS_SLEEP_STATS_V4_MODULE=1
ccflags-y += -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../sched-walt/Module.symvers $(src)/../qcom-smem/Module.symvers \
	$(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-smp2p/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
