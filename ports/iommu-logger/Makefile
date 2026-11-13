# SPDX-License-Identifier: GPL-2.0-only
obj-m += iommu_logger.o
iommu_logger-y := iommu-logger.o
ccflags-y += -I$(src)/../qcom-iommu-util/include -DCONFIG_QTI_IOMMU_SUPPORT_MODULE=1 -DCONFIG_IOMMU_IO_PGTABLE_FAST=1
