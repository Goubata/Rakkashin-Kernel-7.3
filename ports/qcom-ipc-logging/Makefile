# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_ipc_logging.o
qcom_ipc_logging-y := ipc_logging.o ipc_logging_debug.o
ccflags-y += -I$(src)/include -I$(src) -DCONFIG_IPC_LOGGING_MODULE=1 -DCONFIG_QCOM_MINIDUMP_MODULE=1 -DCONFIG_IPC_LOG_MINIDUMP_BUFFERS=16
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-minidump/Module.symvers
