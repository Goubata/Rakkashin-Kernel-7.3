# SPDX-License-Identifier: GPL-2.0-only
obj-m += spmi-pmic-arb.o spmi-pmic-arb-debug.o
ccflags-y += -I$(src)/include -DCONFIG_SPMI_MSM_PMIC_ARB_MODULE=1
obj-m += qti-regmap-debugfs.o qcom-spmi-pmic.o
ccflags-y += -I$(srctree) -DCONFIG_REGMAP_QTI_DEBUGFS_MODULE=1
