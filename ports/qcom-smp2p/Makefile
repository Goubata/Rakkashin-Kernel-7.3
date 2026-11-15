# SPDX-License-Identifier: GPL-2.0-only
obj-m += smp2p.o
ccflags-y += -I$(src) -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers
