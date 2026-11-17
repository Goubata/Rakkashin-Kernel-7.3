# SPDX-License-Identifier: GPL-2.0-only
# canoe USB (canoe_perf.bzl =m): dwc3-msm glue, SS QMP PHY, Synopsys eUSB2/HS PHYs,
# EMU PHY, eUSB2 repeater (phy framework), EUD, USB redrivers.
obj-m += dwc3-msm.o phy-msm-ssusb-qmp.o phy-msm-snps-eusb2.o phy-msm-snps-hs.o phy-qcom-emu.o \
	phy-qcom-eusb2-repeater.o eud.o redriver.o nb7vpq904m.o ps5169.o
dwc3-msm-y := debug_ipc.o dwc3-msm-core.o dwc3-msm-ops.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../usb-repeater/include \
	-I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-interconnect/include \
	-I$(src)/../qcom-clock-core/include
ccflags-y += -DCONFIG_USB_DWC3_MSM_MODULE=1 -DCONFIG_USB_REDRIVER_MODULE=1 -DCONFIG_USB_REPEATER_MODULE=1 \
	-DCONFIG_QCOM_EUD_MODULE=1
ccflags-y += -Wno-missing-prototypes -ferror-limit=0
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers $(src)/../usb-repeater/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers
