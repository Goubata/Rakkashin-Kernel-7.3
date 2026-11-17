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
ccflags-y += -DCONFIG_MMI_PHONE_CASE_SUPPORT=1 -DCONFIG_QTI_PMIC_GLINK_MODULE=1
ccflags-y += -Wno-missing-prototypes -ferror-limit=0
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pmic-glink/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers \
	$(src)/../moto-info/Module.symvers $(src)/../moto-touch-deps/Module.symvers \
	$(src)/../qcom-reboot/Module.symvers
