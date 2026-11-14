# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_glink.o qcom_glink_smem.o rpmsg_ctrl.o
qcom_glink-y := qcom_glink_native.o qcom_glink_ssr.o
ccflags-y += -I$(src) -I$(srctree)/drivers/rpmsg
ccflags-y += -DCONFIG_RPMSG_QCOM_GLINK_MODULE=1 -DCONFIG_RPMSG_QCOM_GLINK_SMEM_MODULE=1 -DCONFIG_RPMSG_CTRL_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers
