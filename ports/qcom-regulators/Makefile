# SPDX-License-Identifier: GPL-2.0-only
obj-m += rpmh-regulator.o debug-regulator.o stub-regulator.o
ccflags-y += -I$(src)/../qcom-clock-core/include -I$(src)/../qcom-rpmh/include -I$(src)/../qcom-ipc-logging/include -I$(srctree)
ccflags-y += -DCONFIG_REGULATOR_DEBUG_CONTROL_MODULE=1 -DCONFIG_REGULATOR_PROXY_CONSUMER_MODULE=1 -DCONFIG_IPC_LOGGING_MODULE=1 -DCONFIG_QCOM_RPMH_MODULE=1 -DCONFIG_QCOM_COMMAND_DB_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers
