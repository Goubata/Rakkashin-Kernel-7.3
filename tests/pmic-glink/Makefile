# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-pmic-glink-test.o
ccflags-y += -I$(src)/../../ports/qcom-pmic-glink/include -I$(src)/../../ports/qcom-ipc-logging/include
ccflags-y += -DCONFIG_QTI_PMIC_GLINK_MODULE=1 -DCONFIG_QCOM_RPROC_COMMON_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-rproc-common/Module.symvers $(src)/../../ports/qcom-ipc-logging/Module.symvers
