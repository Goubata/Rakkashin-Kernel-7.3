# SPDX-License-Identifier: GPL-2.0-only
obj-m += cambistmclkcc-canoe.o camcc-alor.o camcc-canoe.o camcc-chora.o clk-dummy.o clk-rpmh.o debugcc-alor.o debugcc-canoe.o debugcc-chora.o dispcc-canoe.o dispcc-chora.o evacc-canoe.o gcc-alor.o gcc-canoe.o gcc-chora.o gpucc-canoe.o gpucc-chora.o tcsrcc-canoe.o tcsrcc-chora.o videocc-canoe.o videocc-chora.o
ccflags-y += -I$(src) -I$(src)/include
ccflags-y += -DCONFIG_QCOM_CRM_V2_MODULE=1 -DCONFIG_QCOM_RPMH_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers
ccflags-y += -I$(srctree)
ccflags-y += -DCONFIG_QCOM_COMMAND_DB_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-rpmh/Module.symvers
