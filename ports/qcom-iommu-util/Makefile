# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_iommu_util.o
qcom_iommu_util-y := qcom-iommu-util.o qcom-dma-iommu-generic.o io-pgtable-fast.o dma-mapping-fast.o qcom-io-pgtable-arm.o qcom-io-pgtable-alloc.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree)
ccflags-y += -DCONFIG_QCOM_IOMMU_UTIL_MODULE=1 -DCONFIG_IOMMU_IO_PGTABLE_FAST=1 -DCONFIG_IOMMU_IO_PGTABLE_CONTIG_HINT=1 -DCONFIG_QCOM_SECURE_BUFFER_MODULE=1
