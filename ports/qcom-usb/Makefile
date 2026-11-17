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
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
