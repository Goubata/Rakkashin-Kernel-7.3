# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-fake-sdam.o avr-minidump-stub.o
ccflags-y += -I$(src)/../../ports/qcom-minidump/include -DCONFIG_QCOM_MINIDUMP_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-tee/fake-scm/Module.symvers
