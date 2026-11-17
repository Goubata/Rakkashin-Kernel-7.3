# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/camera-kernel (tag MMI-W3WWL36.65-11-18) camera.ko for canoe.
# Sources/defines: Makefile.generated (scripts/gen_camera_makefile.py).
include $(src)/Makefile.generated
ccflags-y += $(AVR_CAMERA_DEFINES) $(AVR_CAMERA_INCLUDES)
ccflags-y += -I$(src)/include/uapi/camera -I$(src)/include/uapi/camera/media -I$(src)/drivers -I$(src)
ccflags-y += -include $(src)/cam_generated.h -Wno-missing-prototypes -Wno-macro-redefined
ccflags-y += -I$(src)/../mm-drivers/include -I$(src)/../securemsm/include -I$(src)/../securemsm/include/linux \
	-I$(src)/../securemsm/smmu-proxy/include/uapi -I$(src)/../securemsm/smmu-proxy \
	-I$(src)/../qti-fence/synx/msm/synx -I$(src)/../qti-fence/synx/include/uapi/synx/media -I$(src)/../qti-fence/include \
	-I$(src)/../qcom-socinfo/include -I$(src)/../qcom-iommu-util/include -I$(src)/../qcom-dma-heaps/include \
	-I$(src)/../qcom-clock-core/include -I$(src)/../qcom-rpmh/include -I$(src)/../qcom-debug-diag/include \
	-I$(src)/../qcom-minidump/include -I$(src)/../qcom-interconnect/include
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
ccflags-y += -include $(src)/../qcom-clock-core/qcom-vendor-clock.h -include $(src)/../qcom-rpmh/rpmh-vendor-api.h \
	-include $(src)/../msm-drm/avr-llcc-display.h
ccflags-y += -I$(src)/avr-include -I$(src)/../securemsm -include $(src)/avr-camera-compat.h
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
