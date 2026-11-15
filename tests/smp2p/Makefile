# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-smp2p-test.o
ccflags-y += -I$(src)/../../ports/qcom-smp2p -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-smem/Module.symvers
