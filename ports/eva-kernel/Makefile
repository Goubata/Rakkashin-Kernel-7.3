# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/eva-kernel msm-eva.ko for canoe (canoe.bzl config_options as
# local defines: TARGET_SYNX_ENABLE TARGET_DSP_ENABLE TARGET_MMRM_ENABLE CONFIG_EVA_CANOE CONFIG_MSM_MMRM).
obj-m += msm-eva.o
msm-eva-y := msm/eva/cvp.o msm/eva/cvp_core_hfi.o msm/eva/cvp_dump.o msm/eva/cvp_fw_load.o msm/eva/cvp_hfi.o msm/eva/cvp_power.o msm/eva/cvp_presil.o msm/eva/cvp_smem.o msm/eva/hfi_packetization.o msm/eva/hfi_response_handler.o msm/eva/msm_cvp.o msm/eva/msm_cvp_buf.o msm/eva/msm_cvp_clocks.o msm/eva/msm_cvp_common.o msm/eva/msm_cvp_core.o msm/eva/msm_cvp_debug.o msm/eva/msm_cvp_dsp.o msm/eva/msm_cvp_ioctl.o msm/eva/msm_cvp_platform.o msm/eva/msm_cvp_res_parse.o msm/eva/msm_cvp_synx.o msm/eva/msm_cvp_sw_dbg.o msm/eva/vm/cvp_vm_main.o msm/eva/vm/cvp_vm_msgq.o msm/eva/vm/cvp_vm_resource.o msm/eva/target/cvp_kaanapali_hal.o msm/eva/target/cvp_pakala_hal.o msm/eva/target/cvp_hawi_hal.o 
# vendor CONFIGs first: later force-included headers (llcc) depend on them
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
ccflags-y += -DTARGET_SYNX_ENABLE=1 -DTARGET_DSP_ENABLE=1 -DTARGET_MMRM_ENABLE=1 -DCONFIG_EVA_CANOE=1 -DCONFIG_MSM_MMRM=1
ccflags-y += -Wno-format -Wno-array-bounds -Wno-missing-prototypes -Wno-macro-redefined
ccflags-y += -I$(src)/config -I$(src)/include/uapi/eva -I$(src)/msm -I$(src)/msm/eva \
	-I$(src)/../mm-drivers/include -I$(src)/../qti-fence/synx/msm/synx -I$(src)/../qti-fence/synx/include/uapi/synx/media \
	-I$(src)/../qti-fence/include -I$(src)/../dsp-kernel/include/linux -I$(src)/../dsp-kernel/include/uapi \
	-I$(src)/../qcom-dma-heaps/include -I$(src)/../qcom-iommu-util/include -I$(src)/../qcom-socinfo/include \
	-I$(src)/../gunyah/include -I$(src)/../gunyah-rm/include -I$(src)/../qcom-clock-core/include -I$(src)/../qcom-dcvs/include -I$(src)/../secure-buffer/include
ccflags-y += -include $(src)/../qcom-clock-core/qcom-vendor-clock.h -include $(src)/../msm-drm/avr-llcc-display.h
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
KBUILD_EXTRA_SYMBOLS += $(src)/../dsp-kernel/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers
