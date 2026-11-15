# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-watchdog-mmio-test.o avr-sdei-test.o avr-watchdog-lifetime-test.o avr-watchdog-core-test.o
ccflags-y += -I$(src)/../../ports/qcom-watchdog/include -I$(srctree) -I$(src)/../../ports/qcom-minidump/include -DCONFIG_QCOM_MINIDUMP_MODULE=1
ccflags-y += -DCONFIG_QCOM_WDT_CORE_MODULE=1 -DCONFIG_QCOM_SDEI_MODULE=1 -DCONFIG_QCOM_WATCHDOG_BARK_TIME=11000 -DCONFIG_QCOM_WATCHDOG_PET_TIME=9360
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-minidump/Module.symvers
ccflags-y += -DCONFIG_QCOM_WATCHDOG_IPI_PING=1
