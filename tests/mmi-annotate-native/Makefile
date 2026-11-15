# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-mmi-annotate-native-test.o
ccflags-y += -I$(src)/../../ports/moto-info/include -I$(src)/../../ports/qcom-minidump/include -DCONFIG_QCOM_MINIDUMP_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-minidump/Module.symvers
