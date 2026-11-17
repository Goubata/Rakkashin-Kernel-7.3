# SPDX-License-Identifier: GPL-2.0-only
# AVR 7.3 port of vendor/qcom/opensource/display-drivers msm_drm for canoe.
# Sources = display_modules.bzl config_srcs for the canoe target config_options
# (targets/canoe.bzl); defines = those options (ddk local_defines). Vendor Kbuild kept as Kbuild.vendor.
obj-m += msm_drm.o
msm_drm-y := msm/msm_atomic.o \
	msm/msm_fb.o \
	msm/msm_drv.o \
	msm/msm_gem.o \
	msm/msm_gem_prime.o \
	msm/msm_gem_vma.o \
	msm/msm_smmu.o \
	msm/msm_prop.o \
	msm/sde/sde_crtc.o \
	msm/sde/sde_encoder.o \
	msm/sde/sde_encoder_dce.o \
	msm/sde/sde_encoder_phys_vid.o \
	msm/sde/sde_encoder_phys_cmd.o \
	msm/sde/sde_irq.o \
	msm/sde/sde_core_irq.o \
	msm/sde/sde_core_perf.o \
	msm/sde/sde_rm.o \
	msm/sde/sde_kms_utils.o \
	msm/sde/sde_kms.o \
	msm/sde/sde_plane.o \
	msm/sde/sde_connector.o \
	msm/sde/sde_color_processing.o \
	msm/sde/sde_vbif.o \
	msm/sde_dbg.o \
	msm/sde_dbg_evtlog.o \
	msm/sde_io_util.o \
	msm/sde_vm_event.o \
	msm/sde/sde_hw_reg_dma_v1_color_proc.o \
	msm/sde/sde_hw_color_proc_v4.o \
	msm/sde/sde_hw_ad4.o \
	msm/sde/sde_hw_uidle.o \
	msm/sde_edid_parser.o \
	msm/sde/sde_hw_catalog.o \
	msm/sde/sde_hw_cdm.o \
	msm/sde/sde_hw_dspp.o \
	msm/sde/sde_hw_intf.o \
	msm/sde/sde_hw_lm.o \
	msm/sde/sde_hw_ctl.o \
	msm/sde/sde_hw_util.o \
	msm/sde/sde_hw_sspp_v1.o \
	msm/sde/sde_hw_sspp.o \
	msm/sde/sde_hw_wb.o \
	msm/sde/sde_hw_pingpong.o \
	msm/sde/sde_hw_top.o \
	msm/sde/sde_hw_interrupts.o \
	msm/sde/sde_hw_vbif.o \
	msm/sde/sde_formats.o \
	msm/sde_power_handle.o \
	msm/sde/sde_hw_color_processing_v1_7.o \
	msm/sde/sde_reg_dma.o \
	msm/sde/sde_hw_reg_dma_v1.o \
	msm/sde/sde_hw_dsc.o \
	msm/sde/sde_hw_dsc_1_2.o \
	msm/sde/sde_hw_vdc.o \
	msm/sde/sde_hw_ds.o \
	msm/sde/sde_fence.o \
	msm/sde/sde_hw_qdss.o \
	msm/sde_dsc_helper.o \
	msm/sde_vdc_helper.o \
	msm/sde/sde_hw_dnsc_blur.o \
	msm/sde/sde_hw_rc.o \
	msm/sde/sde_color_proc_property_helper.o \
	msm/sde/sde_color_processing_aiqe.o \
	msm/sde/sde_hw_color_proc_aiqe_v1.o \
	msm/sde/sde_aiqe_common.o \
	msm/sde/sde_loopback.o \
	msm/sde/sde_motUtil.o \
	msm/sde/sde_color_proc_feature_state_helper.o \
	msm/dsi/dsi_phy.o \
	msm/dsi/dsi_pwr.o \
	msm/dsi/dsi_phy_hw_v3_0.o \
	msm/dsi/dsi_phy_hw_v4_0.o \
	msm/dsi/dsi_phy_hw_v5_0.o \
	msm/dsi/dsi_phy_hw_v7_2.o \
	msm/dsi/dsi_phy_timing_calc.o \
	msm/dsi/dsi_phy_timing_v3_0.o \
	msm/dsi/dsi_phy_timing_v4_0.o \
	msm/dsi/dsi_pll.o \
	msm/dsi/dsi_pll_5nm.o \
	msm/dsi/dsi_pll_4nm.o \
	msm/dsi/dsi_pll_3nm.o \
	msm/dsi/dsi_ctrl_hw_cmn.o \
	msm/dsi/dsi_ctrl_hw_2_2.o \
	msm/dsi/dsi_ctrl.o \
	msm/dsi/dsi_catalog.o \
	msm/dsi/dsi_drm.o \
	msm/dsi/dsi_display.o \
	msm/dsi/dsi_display_manager.o \
	msm/dsi/dsi_panel.o \
	msm/dsi/dsi_clk_manager.o \
	msm/dsi/dsi_display_test.o \
	msm/dsi/dsi_display_mot_ext.o \
	msm/hfi/hfi_adapter.o \
	msm/hfi/hfi_packer.o \
	msm/hfi/hfi_unpacker.o \
	msm/hfi/hfi_utils.o \
	msm/hfi/hfi_msm_drv.o \
	msm/hfi/hfi_dbg.o \
	msm/sde/hfi/hfi_catalog.o \
	msm/sde/hfi/hfi_connector.o \
	msm/sde/hfi/hfi_crtc.o \
	msm/sde/hfi/hfi_encoder.o \
	msm/sde/hfi/hfi_kms.o \
	msm/sde/hfi/hfi_plane.o \
	msm/sde/hfi/hfi_color_proc.o \
	msm/dsi/hfi/dsi_display_hfi.o \
	msm/dsi/hfi/dsi_hfi.o \
	msm/sde/hfi/hfi_wb.o \
	msm/dp/dp_altmode.o \
	msm/dp/dp_parser.o \
	msm/dp/dp_power.o \
	msm/dp/dp_catalog.o \
	msm/dp/dp_catalog_v420.o \
	msm/dp/dp_catalog_v200.o \
	msm/dp/dp_catalog_v500.o \
	msm/dp/dp_aux.o \
	msm/dp/dp_panel.o \
	msm/dp/dp_panel_tu.o \
	msm/dp/dp_link.o \
	msm/dp/dp_ctrl.o \
	msm/dp/dp_audio.o \
	msm/dp/dp_debug.o \
	msm/dp/dp_hpd.o \
	msm/dp/dp_aux_bridge.o \
	msm/dp/dp_bridge_hpd.o \
	msm/dp/dp_mst_sim.o \
	msm/dp/dp_mst_sim_helper.o \
	msm/dp/dp_gpio_hpd.o \
	msm/dp/dp_lphw_hpd.o \
	msm/dp/dp_display.o \
	msm/dp/dp_drm.o \
	msm/dp/dp_pll.o \
	msm/dp/dp_pll_5nm.o \
	msm/dp/dp_pll_4nm.o \
	msm/dp/dp_pll_3nm.o \
	msm/dp/edp_pll_5nm.o \
	msm/dp/edp_pll_4nm.o \
	msm/dp/dp_mst_drm.o \
	msm/dsi/dsi_parser.o \
	msm/sde/sde_wb.o \
	msm/sde/sde_encoder_phys_wb.o \
	hdcp/msm_hdcp.o \
	msm/dp/dp_hdcp2p2.o \
	msm/sde_hdcp_1x.o \
	msm/sde_hdcp_2x.o \
	msm/sde/sde_vm_common.o \
	msm/sde/sde_vm_primary.o \
	msm/sde/sde_vm_trusted.o \
	msm/sde/sde_vm_msgq.o \
	msm/msm_cooling_device.o \
	msm/sde_rsc.o \
	msm/sde_rsc_hw.o \
	msm/sde_rsc_hw_v3.o \
	msm/sde_cesta.o \
	msm/sde_cesta_hw.o
ccflags-y += -DCONFIG_DRM_MSM=1 -DCONFIG_DRM_MSM_SDE=1 -DCONFIG_SYNC_FILE=1 -DCONFIG_DRM_MSM_DSI=1 -DCONFIG_QTI_HFI_CORE=1 -DCONFIG_MDSS_HFI_ADAPTER=1 -DCONFIG_MDSS_HFI=1 -DCONFIG_DRM_MSM_DP=1 -DCONFIG_DRM_MSM_DP_MST=1 -DCONFIG_DSI_PARSER=1 -DCONFIG_DRM_SDE_WB=1 -DCONFIG_DRM_MSM_REGISTER_LOGGING=1 -DCONFIG_QCOM_MDSS_PLL=1 -DCONFIG_HDCP_QSEECOM=1 -DCONFIG_DRM_SDE_VM=1 -DCONFIG_QCOM_WCD939X_I2C=1 -DCONFIG_THERMAL_OF=1 -DCONFIG_QCOM_SPEC_SYNC=1 -DCONFIG_MSM_EXT_DISPLAY=1 -DCONFIG_DRM_SDE_RSC=1 -DCONFIG_DRM_SDE_CESTA=1 -DCONFIG_QTI_HW_FENCE=1 -DCONFIG_DSI_EXTENDED_MODES=1
ccflags-y += -I$(src) -I$(src)/include -I$(src)/include/linux -I$(src)/include/uapi/display \
	-I$(src)/msm -I$(src)/msm/dp -I$(src)/msm/dsi -I$(src)/msm/hfi -I$(src)/msm/sde/hfi \
	-I$(src)/msm/dsi/hfi -I$(src)/msm/sde
ccflags-y += -DANI_LITTLE_BYTE_ENDIAN -DANI_LITTLE_BIT_ENDIAN -DDOT11F_LITTLE_ENDIAN_HOST \
	-DANI_COMPILER_TYPE_GCC -DANI_OS_TYPE_ANDROID=6 -DPTT_SOCK_SVC_ENABLE -D__linux__
ccflags-y += -Wno-missing-prototypes
# vendor dependency headers (ported modules)
ccflags-y += -I$(src)/../mm-drivers/msm_ext_display/include -I$(src)/../mm-drivers/sync_fence/include \
	-I$(src)/../mm-drivers/hfi_core/inc -I$(src)/../qti-fence/hw_fence/include \
	-I$(src)/../qti-fence/include -I$(src)/../securemsm/include -I$(src)/../securemsm/include/linux \
	-I$(src)/../securemsm/smmu-proxy/include/uapi -I$(src)/../securemsm/smmu-proxy \
	-I$(src)/../qcom-display-deps/include
ccflags-y += -I$(src)/../mm-drivers/include -I$(src)/../qcom-iommu-util/include -I$(src)/../qcom-socinfo/include -I$(src)/../qcom-panel-event/include -I$(src)/../qcom-clock-core/include -I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-minidump/include -I$(src)/../qcom-debug-diag/include -I$(src)/../qcom-dma-heaps/include -I$(src)/../qcom-usb/include -I$(src)/../qcom-charging/include -I$(src)/../qcom-rpmh/include -I$(src)/../gunyah/include -I$(src)/../gunyah-rm/include -I$(src)/../secure-buffer/include -I$(src)/../qcom-pmic-glink/include 
ccflags-y += -include $(src)/avr-canoe-vendor-config.h -include $(src)/avr-drm-compat.h
ccflags-y += -I$(src)/../qti-fence/synx/msm/synx -I$(src)/../qcom-dcvs/include -I$(src)/../usb-repeater/include -I$(src)/../qcom-spmi/include -I$(src)/../pinctrl-alor-vendor-core/include
ccflags-y += -I$(src)/../qti-fence/synx/include/uapi/synx/media
# vendor files define pr_fmt after the force-included compat header pulled printk.h in;
# the later definition is the one used at expansion, as in the vendor build.
ccflags-y += -Wno-macro-redefined
# vendor Kbuild also searches $(KERNEL_SRC)/include/linux and .../drm; keep them after
# the vendor dependency headers so e.g. <soc/qcom/socinfo.h> resolves to the vendor one
ccflags-y += -I$(src)/msm/hdmi -I$(src)/rotator -include $(src)/../qcom-interconnect/include/dt-bindings/interconnect/qcom,icc.h -I$(src)/../qcom-interconnect/include -I$(srctree)/include/linux -I$(srctree)/include/drm
# vendor APIs whose declarations live in headers shadowed by 7.3 ones (providers ported)
ccflags-y += -include $(src)/../qcom-clock-core/qcom-vendor-clock.h -include $(src)/../qcom-rpmh/rpmh-vendor-api.h \
	-include $(src)/avr-llcc-display.h -DCONFIG_QCOM_SOCINFO_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../drm-display-helper/Module.symvers $(src)/../mm-drivers/Module.symvers \
	$(src)/../securemsm/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../qcom-socinfo/Module.symvers \
	$(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-iommu-util/Module.symvers \
	$(src)/../qcom-clock-core/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers \
	$(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-charging/Module.symvers \
	$(src)/../qcom-usb/Module.symvers $(src)/../qcom-panel-event/Module.symvers \
	$(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers \
	$(src)/../qcom-dma-heaps/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers \
	$(src)/../qcom-minidump/Module.symvers $(src)/../qcom-spmi/Module.symvers
