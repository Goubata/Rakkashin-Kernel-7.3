# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-tee-test.o
ccflags-y += -I$(src) -I$(src)/../../ports/qcom-tee/include
KBUILD_EXTRA_SYMBOLS += $(src)/fake-scm/Module.symvers $(src)/../../ports/qcom-tee/Module.symvers
