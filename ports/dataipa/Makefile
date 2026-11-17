# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/dataipa (gsim, ipam, ipanetm; canoe perf defconfig) and
# datarmnet-ext/mem rmnet_mem (ipam dependency, CONFIG_IPA_RMNET_MEM) on android18-7.3.
include $(src)/Makefile.generated
obj-m += rmnet_mem.o
rmnet_mem-y := rmnet_mem/rmnet_mem_main.o rmnet_mem/rmnet_mem_nl.o rmnet_mem/rmnet_mem_pool.o
CFLAGS_rmnet_mem/rmnet_mem_main.o += -I$(src)/rmnet_mem -I$(src)/rmnet_mem/include/uapi/linux
CFLAGS_rmnet_mem/rmnet_mem_nl.o += -I$(src)/rmnet_mem -I$(src)/rmnet_mem/include/uapi/linux
CFLAGS_rmnet_mem/rmnet_mem_pool.o += -I$(src)/rmnet_mem -I$(src)/rmnet_mem/include/uapi/linux
ccflags-y += $(AVR_IPA_DEFINES) -DCONFIG_IPA_RMNET_MEM=1
# trace headers are found through the -I paths below (TRACE_INCLUDE_PATH "."), as
# the DDK passes their package directories
ccflags-y += -DGSI_TRACE_INCLUDE_PATH=. -DIPA_TRACE_INCLUDE_PATH=. -DRNDIS_TRACE_INCLUDE_PATH=. -DNCM_TRACE_INCLUDE_PATH=.
ccflags-y += -I$(src)/avr-include
ccflags-y += -I$(src)/drivers/platform/msm/include -I$(src)/drivers/platform/msm/include/linux -I$(src)/drivers/platform/msm/include/uapi \
	-I$(src)/drivers/platform/msm/gsi -I$(src)/drivers/platform/msm/gsi/gsihal -I$(src)/drivers/platform/msm/ipa -I$(src)/drivers/platform/msm/ipa/ipa_test_module \
	-I$(src)/drivers/platform/msm/ipa/ipa_v3 -I$(src)/drivers/platform/msm/ipa/ipa_v3/ipahal -I$(src)/drivers/platform/msm/ipa/ipa_clients -I$(src)/config \
	-I$(src)/rmnet_mem -I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-iommu-util/include \
	-I$(src)/../qcom-wlan-deps/include -I$(src)/../qcom-rproc-common/include -I$(src)/../qcom-debug-diag/include \
	-I$(src)/../qcom-usb-gadget/include -I$(src)/../qcom-minidump/include -I$(src)/../qcom-smem/include
ccflags-y += -Wno-missing-prototypes -Wno-macro-redefined
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-iommu-util/Module.symvers \
	$(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers \
	$(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-debug-diag/Module.symvers \
	$(src)/../qcom-usb-gadget/Module.symvers $(src)/../qcom-smem/Module.symvers \
	$(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-minidump/Module.symvers
