# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-interconnect-test.o
ccflags-y += -I$(src)/../../ports/qcom-interconnect -I$(src)/../../ports/qcom-clock-core/include -I$(srctree)
ccflags-y += -include $(src)/../../ports/qcom-interconnect/include/dt-bindings/interconnect/qcom,icc.h
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-interconnect/Module.symvers
