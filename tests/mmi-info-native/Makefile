# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-mmi-info-native-test.o
ccflags-y += -I$(src)/../../ports/moto-info/include -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/moto-info/Module.symvers $(src)/../../ports/qcom-smem/Module.symvers
