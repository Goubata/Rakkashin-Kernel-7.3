# SPDX-License-Identifier: GPL-2.0-only
obj-m += arm_smmu.o
arm_smmu-y := arm-smmu.o arm-smmu-impl.o arm-smmu-nvidia.o arm-smmu-qcom.o arm-smmu-qcom-pm.o
ccflags-y += -I$(src)/include -I$(src) -I$(src)/../qcom-iommu-util/include -I$(srctree)
ccflags-y += -DCONFIG_ARM_SMMU_MODULE=1 -DCONFIG_ARM_SMMU_QCOM=1 -DCONFIG_ARM_SMMU_DISABLE_BYPASS_BY_DEFAULT=1
ccflags-y += -DCONFIG_QCOM_IOMMU_UTIL_MODULE=1 -DCONFIG_QTI_IOMMU_SUPPORT_MODULE=1 -DCONFIG_QCOM_SECURE_BUFFER_MODULE=1
ccflags-y += -DCONFIG_IOMMU_IO_PGTABLE_FAST=1 -DCONFIG_IOMMU_IO_PGTABLE_CONTIG_HINT=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-iommu-util/Module.symvers $(src)/../qcom-socinfo/Module.symvers

KBUILD_EXTRA_SYMBOLS += $(src)/../iommu-logger/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers $(src)/../geni-bus/Module.symvers $(src)/../qcom-thermal-misc/Module.symvers $(src)/../qcom-firststage-misc/Module.symvers
