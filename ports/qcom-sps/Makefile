# SPDX-License-Identifier: GPL-2.0-only
# qcom-soc drivers/soc/qcom/sps (canoe_perf.bzl: CONFIG_SPS=m, CONFIG_SPS_SUPPORT_NDP_BAM=y)
obj-m += sps_drv.o
sps_drv-y := bam.o sps.o sps_bam.o sps_dma.o sps_map.o sps_mem.o sps_rm.o
ccflags-y += -I$(src)/include -I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-minidump/include \
	-I$(src)/../qcom-iommu-util/include -I$(src)/../qcom-dma-heaps/include -I$(src)/../secure-buffer/include
ccflags-y += -DCONFIG_SPS_MODULE=1 -DCONFIG_SPS_SUPPORT_NDP_BAM=1 -DCONFIG_IPC_LOGGING_MODULE=1 \
	-DCONFIG_QCOM_MINIDUMP_MODULE=1 -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-minidump/Module.symvers \
	$(src)/../qcom-iommu-util/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers
