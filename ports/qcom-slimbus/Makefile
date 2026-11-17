# SPDX-License-Identifier: GPL-2.0-only
# qcom-soc 6.12 vendor sources (canoe_perf: CONFIG_SLIMBUS=m, CONFIG_SLIM_QCOM_NGD_CTRL=m,
# CONFIG_QCOM_BAM_DMA=m): vendor-modified slimbus core (stream unprepare/disconnect
# API used by btfm_slim_codec), NGD controller and BAM DMA.
obj-m += slimbus.o slim-qcom-ngd-ctrl.o bam_dma.o
slimbus-y := core.o messaging.o sched.o stream.o
slim-qcom-ngd-ctrl-y := qcom-ngd-ctrl.o
bam_dma-y := dma/bam_dma.o
CFLAGS_qcom-ngd-ctrl.o += -I$(src)
CFLAGS_dma/bam_dma.o += -I$(src)/dma -I$(srctree)
ccflags-y += -I$(src)/include -I$(src)/../qcom-charging/include -I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-rproc-common/include \
	-I$(src)/../qcom-minidump/include -Wno-missing-prototypes \
	-DCONFIG_SLIMBUS_MODULE=1 -DCONFIG_SLIM_QCOM_NGD_CTRL_MODULE=1 -DCONFIG_QCOM_BAM_DMA_MODULE=1 \
	-DCONFIG_IPC_LOGGING_MODULE=1 -DCONFIG_QCOM_RPROC_COMMON_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-rproc-common/Module.symvers \
	$(src)/../qcom-remoteproc-services/Module.symvers
CFLAGS_stream.o += -include $(src)/include/linux/slimbus_vendor.h
