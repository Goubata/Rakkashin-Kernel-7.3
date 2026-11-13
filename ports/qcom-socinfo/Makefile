# SPDX-License-Identifier: GPL-2.0-only
obj-m += socinfo.o
ccflags-y += -I$(src)/include -I$(srctree) -DCONFIG_QCOM_SMEM_MODULE=1 -DCONFIG_QCOM_SOCINFO_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers
