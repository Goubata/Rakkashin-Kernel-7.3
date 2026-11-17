# SPDX-License-Identifier: GPL-2.0-only
# Motorola charging modules loaded on the AVR (motorola-kernel-modules):
# mmi_glink_charger (+ MMI_PHONE_CASE_SUPPORT: it depends on hall_phone_case,
# which the phone loads), qti_glink_charger_v2, qpnp_adaptive_charge,
# bm_adsp_ulog, hall_phone_case.
obj-m += mmi_glink_charger.o qti_glink_charger_v2.o qpnp_adaptive_charge.o bm_adsp_ulog.o \
	hall_phone_case.o
mmi_glink_charger-y := mmi_glink_charger/battery_host.o mmi_glink_charger/device_class.o \
	mmi_glink_charger/mmi_glink_core.o mmi_glink_charger/trusted_shash_lib.o \
	mmi_glink_charger/device_glink/balance_charge_glink.o mmi_glink_charger/device_glink/battery_glink.o \
	mmi_glink_charger/device_glink/charge_pump_glink.o mmi_glink_charger/device_glink/switch_buck_glink.o \
	mmi_glink_charger/device_glink/usb_glink.o mmi_glink_charger/device_glink/wireless_charge_glink.o
qti_glink_charger_v2-y := qti_glink_charger_v2/qti_glink_charger_v2.o
qpnp_adaptive_charge-y := qpnp_adaptive_charge/qpnp_adaptive_charge.o
bm_adsp_ulog-y := bm_adsp_ulog/bm_adsp_ulog.o
hall_phone_case-y := hall/hall_phone_case.o
ccflags-y += -I$(src)/include -I$(src)/mmi_glink_charger -I$(src)/mmi_glink_charger/device_glink \
	-I$(src)/../qcom-pmic-glink/include -I$(src)/../qcom-ipc-logging/include \
	-I$(src)/../moto-info/include -I$(src)/../moto-info/mmi_info -I$(src)/../goodix-touch/include \
	-I$(src)/../moto-touch-deps/include -I$(src)/../qcom-charging/include
ccflags-y += -DCONFIG_QTI_PMIC_GLINK_MODULE=1
ccflags-y += -Wno-missing-prototypes -ferror-limit=0
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pmic-glink/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers \
	$(src)/../moto-info/Module.symvers $(src)/../moto-touch-deps/Module.symvers \
	$(src)/../qcom-reboot/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
