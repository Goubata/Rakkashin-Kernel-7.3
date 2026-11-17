# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-remoteproc-services-test.o
ccflags-y += -I$(src)/../../ports/qcom-remoteproc-services -I$(srctree)/drivers/remoteproc -I$(src)/../../ports/qcom-rproc-common/include
ccflags-y += -DCONFIG_QCOM_SYSMON_MODULE=1 -DCONFIG_QCOM_AOSS_QMP_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-smem/Module.symvers $(src)/../../ports/qcom-remoteproc-services/Module.symvers $(src)/../../ports/qcom-rproc-common/Module.symvers
