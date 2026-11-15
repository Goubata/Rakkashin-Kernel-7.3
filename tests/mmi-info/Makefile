# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-mmi-info-test.o
avr-mmi-info-test-y := main-source.o ram-source.o storage-source.o unit-source.o boot-source.o test.o
ccflags-y += -I$(src)/../../ports/moto-info/include -I$(src)/../../ports/moto-info/mmi_info
ccflags-y += -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/moto-info/Module.symvers
