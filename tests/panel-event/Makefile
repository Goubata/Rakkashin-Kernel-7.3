# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-panel-event-test.o
ccflags-y += -I$(src)/../../ports/qcom-panel-event/include -DCONFIG_QCOM_PANEL_EVENT_NOTIFIER_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-panel-event/Module.symvers
