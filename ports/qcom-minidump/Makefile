# SPDX-License-Identifier: GPL-2.0-only
obj-m += minidump.o
minidump-y := msm_minidump.o minidump_log.o minidump_smem.o minidump_memory.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../gunyah/include
ccflags-y += -DCONFIG_QCOM_MINIDUMP_MODULE=1 -DCONFIG_QCOM_MINIDUMP_SMEM=1 -DCONFIG_QCOM_DEBUG_SYMBOL_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1 -DCONFIG_MINIDUMP_MAX_ENTRIES=200
ccflags-y += -DCONFIG_QCOM_MINIDUMP_PANIC_DUMP=1 -DCONFIG_QCOM_MINIDUMP_PANIC_CPU_CONTEXT=1 -DCONFIG_QCOM_MINIDUMP_PANIC_MEMORY_INFO=1 -DCONFIG_QCOM_MINIDUMP_PANIC_CPUFREQ_INFO=1 -DCONFIG_QCOM_MINIDUMP_PANIC_KTASK_STACK=1 -DCONFIG_QCOM_MINIDUMP_PSTORE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers
# WALT interface (ufs-qcom/minidump_log use walt.h like 6.12)
ccflags-y += -I$(src)/../sched-walt/include -DCONFIG_SCHED_WALT_NR_CPUS=8
KBUILD_EXTRA_SYMBOLS += $(src)/../sched-walt/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
