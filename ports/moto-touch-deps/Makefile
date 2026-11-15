# SPDX-License-Identifier: GPL-2.0-only
obj-m += mmi_relay.o sensors_class.o
ccflags-y += -I$(src)/include -I$(src)/../qcom-minidump/include -DCONFIG_QCOM_MINIDUMP_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-minidump/Module.symvers
