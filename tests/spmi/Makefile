# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-spmi-test.o
ccflags-y += -I$(src)/../../ports/qcom-spmi/include -DCONFIG_SPMI_MSM_PMIC_ARB_MODULE=1 -DCONFIG_REGMAP_QTI_DEBUGFS_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-spmi/Module.symvers
