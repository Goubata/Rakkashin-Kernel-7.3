# SPDX-License-Identifier: GPL-2.0-only
obj-m += clk-qcom.o gdsc-regulator.o
clk-qcom-y := clk-alpha-pll.o clk-branch.o clk-debug.o clk-hfpll.o clk-opp.o clk-pll.o clk-pm.o clk-rcg.o clk-rcg2.o clk-regmap-divider.o clk-regmap-mux-div.o clk-regmap-mux.o clk-regmap-phy-mux.o clk-regmap.o common.o reset.o vdd-class.o gdsc.o gdsc-debug.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree)
obj-m += proxy-consumer.o
# Driver-local configuration, matching Motorola's DDK dependency on this module.
ccflags-y += -DCONFIG_REGULATOR_PROXY_CONSUMER_MODULE=1
ccflags-y += -DCONFIG_QCOM_GDSC_REGULATOR_MODULE=1
obj-m += crm-v2.o
ccflags-y += -DCONFIG_QCOM_CRM_V2_MODULE=1 -DCONFIG_QCOM_RPMH_TIMEOUT=5000
ccflags-y += -I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-minidump/include -I$(src)/../qcom-regulators/include	# vendor-config parity headers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers $(src)/../geni-bus/Module.symvers $(src)/../qcom-thermal-misc/Module.symvers $(src)/../qcom-firststage-misc/Module.symvers
