# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-glink-transport-test.o
ccflags-y += -I$(src)/../../ports/qcom-glink-transport -I$(srctree)/drivers/rpmsg
ccflags-y += -DCONFIG_RPMSG_QCOM_GLINK_MODULE=1 -DCONFIG_RPMSG_QCOM_GLINK_SMEM_MODULE=1 -DCONFIG_RPMSG_CTRL_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-smem/Module.symvers
