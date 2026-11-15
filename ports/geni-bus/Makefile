# SPDX-License-Identifier: GPL-2.0-only
obj-m += msm_gpi.o i2c-msm-geni.o spi-msm-geni.o
ccflags-y += -I$(src)/include -I$(srctree) -I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-smem/include
ccflags-y += -DCONFIG_IPC_LOGGING_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-smem/Module.symvers
