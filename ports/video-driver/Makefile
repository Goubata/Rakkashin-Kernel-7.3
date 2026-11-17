# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/video-driver msm_video for canoe (Makefile.generated).
include $(src)/Makefile.generated
# vendor CONFIGs first: later force-included headers (llcc) depend on them
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
ccflags-y += $(AVR_VIDEO_DEFINES) $(AVR_VIDEO_INCLUDES) -Wno-missing-prototypes -Wno-macro-redefined
ccflags-y += -I$(src)/avr-include -I$(src)/../qcom-dcvs/include -I$(src)/../gunyah/include -I$(src)/../gunyah-rm/include -I$(src)/../mm-drivers/include -I$(src)/../qcom-socinfo/include -I$(src)/../qcom-iommu-util/include \
	-I$(src)/../qcom-dma-heaps/include -I$(src)/../qcom-clock-core/include -I$(src)/../qcom-debug-diag/include \
	-I$(src)/../qcom-minidump/include -I$(src)/../qcom-interconnect/include
ccflags-y += -include $(src)/../qcom-clock-core/qcom-vendor-clock.h -include $(src)/../msm-drm/avr-llcc-display.h
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers
