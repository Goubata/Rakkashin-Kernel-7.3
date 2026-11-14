# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_common.o
ccflags-y += -I$(srctree)/drivers/remoteproc -DCONFIG_QCOM_RPROC_COMMON_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers
ccflags-y += -DCONFIG_RPMSG_QCOM_GLINK_MODULE=1 -DCONFIG_RPMSG_QCOM_GLINK_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-glink-transport/Module.symvers
