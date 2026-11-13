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
