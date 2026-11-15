# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-pas-driver-test.o
ccflags-y += -I$(src)/../../ports/qcom-rproc-common -I$(src)/../../ports/qcom-remoteproc-services -I$(src)/../../ports/qcom-pas-alor -I$(srctree)/drivers/remoteproc
ccflags-y += -DCONFIG_QCOM_SYSMON_MODULE=1 -DCONFIG_QCOM_AOSS_QMP_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1 -DCONFIG_QCOM_PIL_INFO_MODULE=1 -DCONFIG_QCOM_MDT_LOADER_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-pas-alor/Module.symvers $(src)/../../ports/qcom-smem/Module.symvers $(src)/../../ports/qcom-rproc-common/Module.symvers $(src)/../../ports/qcom-remoteproc-services/Module.symvers
