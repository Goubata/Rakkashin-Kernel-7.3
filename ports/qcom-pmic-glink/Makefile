# SPDX-License-Identifier: GPL-2.0-only
obj-m += qti_pmic_glink.o
ccflags-y += -I$(src)/include -I$(src)/../qcom-ipc-logging/include
ccflags-y += -DCONFIG_QTI_PMIC_GLINK_MODULE=1 -DCONFIG_QCOM_RPROC_COMMON_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers
