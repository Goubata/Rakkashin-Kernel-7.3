# SPDX-License-Identifier: GPL-2.0-only
obj-m += touchscreen_mmi.o goodix_brl_thp.o
touchscreen_mmi-y := touchscreen_mmi/touchscreen_mmi_class.o touchscreen_mmi/touchscreen_mmi_gesture.o touchscreen_mmi/touchscreen_mmi_notif.o touchscreen_mmi/touchscreen_mmi_panel.o
goodix_brl_thp-y := goodix_thp/goodix_thp.o goodix_thp/goodix_thp_spi.o goodix_thp/goodix_thp_config.o goodix_thp/goodix_thp_mmi.o
ccflags-y += -I$(src)/include -I$(src)/goodix_thp -I$(src)/../moto-touch-deps/include -I$(src)/../qcom-panel-event/include
ccflags-y += -DCONFIG_QCOM_PANEL_EVENT_NOTIFIER_MODULE=1 -DCONFIG_DRM_PANEL_EVENT_NOTIFICATIONS=1 -DCONFIG_INPUT_TOUCHSCREEN_MMI=1
ccflags-y += -DCONFIG_ENABLE_GTP_PALM_CANCEL=1 -DCONFIG_ENABLE_GTP_PALM_CANCEL_BY_ID=1 -DCONFIG_TOUCHIRQ_UPDATE_QOS=1
ccflags-y += -DCONFIG_TOUCHCLASS_MMI_IRQ_ON_ONCE=1 -DCONFIG_TOUCHSCREEN_DEVICE_VIRTUAL_PATH=1 -DCONFIG_BOARD_USES_DOUBLE_TAP_CTRL=1
KBUILD_EXTRA_SYMBOLS += $(src)/../moto-touch-deps/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../moto-info/Module.symvers
