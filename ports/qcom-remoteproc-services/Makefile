# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_q6v5.o qcom_sysmon.o qcom_aoss.o
ccflags-y += -I$(src) -I$(srctree)/drivers/remoteproc
ccflags-y += -DCONFIG_QCOM_SYSMON_MODULE=1 -DCONFIG_QCOM_AOSS_QMP_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers
