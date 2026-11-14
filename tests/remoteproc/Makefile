# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-rproc-test.o
ccflags-y += -I$(srctree)/drivers/remoteproc -I$(src)/../../ports/qcom-rproc-common -DCONFIG_QCOM_RPROC_COMMON_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-rproc-common/Module.symvers
obj-m += avr-gunyah-test.o
ccflags-y += -I$(src)/../../ports/gunyah/include -DCONFIG_GH_DBL_MODULE=1 -DCONFIG_GH_MSGQ_MODULE=1 -DCONFIG_GH_ARM64_DRV_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/gunyah/Module.symvers
