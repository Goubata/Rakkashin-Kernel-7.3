# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-regulators-test.o
ccflags-y += -I$(src)/../../ports/qcom-clock-core/include -I$(srctree) -DCONFIG_REGULATOR_PROXY_CONSUMER_MODULE=1 -DCONFIG_REGULATOR_DEBUG_CONTROL_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-clock-core/Module.symvers $(src)/../../ports/qcom-regulators/Module.symvers
