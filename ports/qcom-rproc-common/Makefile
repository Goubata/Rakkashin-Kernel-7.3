# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_common.o
ccflags-y += -I$(srctree)/drivers/remoteproc -DCONFIG_QCOM_RPROC_COMMON_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers
ccflags-y += -DCONFIG_RPMSG_QCOM_GLINK_MODULE=1 -DCONFIG_RPMSG_QCOM_GLINK_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-glink-transport/Module.symvers
# Stage120: vendor rproc_qcom_common also defines/exports the rproc_qcom_event
# tracepoint (qcom_tracepoints.c), imported by slim-qcom-ngd-ctrl, qcom_q6v5(_pas),
# qcom_sysmon, msm_memshare, glink_probe on the phone.
qcom_common-y := qcom_common_core.o qcom_tracepoints.o
ccflags-y += -I$(src)/include
