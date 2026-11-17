# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/mm-drivers (firmware = MMI-W3WWL36.65-11-18): canoe display deps
obj-m += msm_ext_display.o sync_fence.o msm_hfi_core.o
msm_ext_display-y := msm_ext_display/src/msm_ext_display.o
sync_fence-y := sync_fence/src/qcom_sync_file.o
msm_hfi_core-y := hfi_core/src/hfi_transport/hfi_queue.o hfi_core/src/hfi_transport/hfi_ipc.o \
	hfi_core/src/hfi_transport/hfi_smmu.o hfi_core/src/hfi_transport/hfi_swi.o \
	hfi_core/src/hfi_transport/hfi_queue_controller.o hfi_core/src/hfi_transport/hfi_if_abstraction.o \
	hfi_core/src/hfi_base/hfi_core.o hfi_core/src/hfi_base/hfi_core_irq.o \
	hfi_core/src/hfi_base/hfi_core_firmware.o hfi_core/src/hfi_base/hfi_core_ssr.o \
	hfi_core/src/hfi_dbg_packet.o hfi_core/src/hfi_core_debug.o hfi_core/src/hfi_core_probe.o
ccflags-y += -I$(src)/include -I$(src)/msm_ext_display/include -I$(src)/sync_fence/include -I$(src)/sync_fence/include/uapi \
	-I$(src)/hfi_core/inc
ccflags-y += -DCONFIG_QCOM_SPEC_SYNC=1 -DCONFIG_QTI_HFI_CORE=1 -DCONFIG_MSM_EXT_DISPLAY=1
ccflags-y += -Wno-missing-prototypes -include $(src)/avr-mm-compat.h
ccflags-y += -DCONFIG_QCOM_SCM_MODULE=1 -DCONFIG_QCOM_MDT_LOADER_MODULE=1 -DCONFIG_QCOM_SMEM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers $(src)/../qcom-pas-alor/Module.symvers
# vendor/qcom/opensource/mmrm-driver (tag MMI-W3WWL36.65-11-18), canoe: CONFIG_MSM_MMRM=y, no VM
obj-m += msm-mmrm.o
msm-mmrm-y := mmrm/driver/src/msm_mmrm.o mmrm/driver/src/mmrm_internal.o mmrm/driver/src/mmrm_res_parse.o \
	mmrm/driver/src/mmrm_debug.o mmrm/driver/src/mmrm_clk_rsrc_mgr_sw.o mmrm/driver/src/mmrm_clk_rsrc_mgr.o
ccflags-y += -I$(src)/mmrm/driver/src -I$(src)/mmrm/driver/inc -DCONFIG_MSM_MMRM=1
# vendor <linux/clk/qcom.h> API (qcom_clk_get_voltage, qcom_clk_crm_set_rate) from clk-qcom.ko
MMRM_CLK := -include $(src)/../qcom-clock-core/qcom-vendor-clock.h -I$(src)/../qcom-clock-core/include
CFLAGS_mmrm/driver/src/mmrm_clk_rsrc_mgr_sw.o += $(MMRM_CLK)
CFLAGS_mmrm/driver/src/mmrm_clk_rsrc_mgr.o += $(MMRM_CLK)
CFLAGS_mmrm/driver/src/mmrm_res_parse.o += $(MMRM_CLK)
CFLAGS_mmrm/driver/src/msm_mmrm.o += $(MMRM_CLK)
CFLAGS_mmrm/driver/src/mmrm_internal.o += $(MMRM_CLK)
CFLAGS_mmrm/driver/src/mmrm_debug.o += $(MMRM_CLK)
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers
