# SPDX-License-Identifier: GPL-2.0-only
# Reboot/download-mode/PON-log modules loaded on the phone (canoe_perf.bzl:
# POWER_RESET_QCOM_DOWNLOAD_MODE=m (+_DEFAULT=y), POWER_RESET_QCOM_REBOOT_REASON=m,
# QTI_PMIC_PON_LOG=m with MOT_PON_LOG (/proc/driver/mot_pon_log exists on the phone),
# QCOM_MINIDUMP=m) + Motorola moto_reboot_reason.
obj-m += qcom-dload-mode.o qcom-reboot-reason.o pmic-pon-log.o moto_reboot_reason.o
ccflags-y += -I$(src)/../qcom-minidump/include -I$(src)/../qcom-ipc-logging/include
ccflags-y += -DCONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE_MODULE=1 \
	-DCONFIG_POWER_RESET_QCOM_DOWNLOAD_MODE_DEFAULT=1 -DCONFIG_QCOM_MINIDUMP_MODULE=1 \
	-DCONFIG_MOT_PON_LOG=1
ccflags-y += -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
