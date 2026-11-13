# SPDX-License-Identifier: GPL-2.0-only
obj-m += arm_smmu.o
arm_smmu-y := arm-smmu.o arm-smmu-impl.o arm-smmu-nvidia.o arm-smmu-qcom.o arm-smmu-qcom-pm.o
ccflags-y += -I$(src)/include -I$(src) -I$(src)/../qcom-iommu-util/include -I$(srctree)
ccflags-y += -DCONFIG_ARM_SMMU_MODULE=1 -DCONFIG_ARM_SMMU_QCOM=1 -DCONFIG_ARM_SMMU_DISABLE_BYPASS_BY_DEFAULT=1
ccflags-y += -DCONFIG_QCOM_IOMMU_UTIL_MODULE=1 -DCONFIG_QTI_IOMMU_SUPPORT_MODULE=1 -DCONFIG_QCOM_SECURE_BUFFER_MODULE=1
ccflags-y += -DCONFIG_IOMMU_IO_PGTABLE_FAST=1 -DCONFIG_IOMMU_IO_PGTABLE_CONTIG_HINT=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-iommu-util/Module.symvers $(src)/../qcom-socinfo/Module.symvers

KBUILD_EXTRA_SYMBOLS += $(src)/../iommu-logger/Module.symvers
