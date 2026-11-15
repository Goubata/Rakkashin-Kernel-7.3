# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_q6v5_pas.o qcom_pil_info.o mdt_loader.o
ccflags-y += -I$(src)/../qcom-rproc-common -I$(src)/../qcom-remoteproc-services -I$(srctree)/drivers/remoteproc
ccflags-y += -DCONFIG_QCOM_SYSMON_MODULE=1 -DCONFIG_QCOM_AOSS_QMP_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1 -DCONFIG_QCOM_PIL_INFO_MODULE=1 -DCONFIG_QCOM_MDT_LOADER_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers
