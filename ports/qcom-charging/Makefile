# SPDX-License-Identifier: GPL-2.0-only
# canoe PMIC glink clients and glink helpers (canoe_perf.bzl =m).
obj-m += qti_battery_charger.o altmode-glink.o ucsi_qti_glink.o charger-ulog-glink.o \
	qti_battery_debug.o qti-glink-adc.o pmic-glink-debug.o glink_pkt.o \
	qcom_pd_mapper.o
qti_battery_charger-y := qti_battery_charger_main.o qti_charger_boost_lib.o qti_typec_class.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../qcom-pmic-glink/include \
	-I$(src)/../qcom-panel-event/include -I$(src)/../qcom-clock-core/include \
	-I$(src)/../qcom-glink-transport -I$(src)/../qcom-glink-transport/include -I$(src)/../qcom-smem/include -I$(src)/../qcom-ipc-logging/include \
	-I$(src)/../qcom-remoteproc-services/include
ccflags-y += -DCONFIG_QTI_PMIC_GLINK_MODULE=1 -DCONFIG_QTI_BATTERY_CHARGER_MODULE=1 \
	-DCONFIG_QTI_ALTMODE_GLINK_MODULE=1 -DCONFIG_RPMSG_QCOM_GLINK_MODULE=1 -DCONFIG_QCOM_PANEL_EVENT_NOTIFIER_MODULE=1
ccflags-y += -Wno-missing-prototypes -ferror-limit=0
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pmic-glink/Module.symvers $(src)/../qcom-panel-event/Module.symvers \
	$(src)/../qcom-glink-transport/Module.symvers $(src)/../qcom-smem/Module.symvers \
	$(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers \
	$(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-rproc-common/Module.symvers
