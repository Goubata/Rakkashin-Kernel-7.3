# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-io-pgtable-test.o
ccflags-y += -I$(src)/../../ports/qcom-iommu-util/include
ccflags-y += -DCONFIG_IOMMU_IO_PGTABLE_FAST=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-iommu-util/Module.symvers
