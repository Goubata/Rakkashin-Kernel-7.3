# SPDX-License-Identifier: GPL-2.0-only
# First-stage modules whose AVR (qcom-soc 6.12) sources carry vendor changes or
# whose 7.3 Kconfig symbols are not selectable here (canoe_perf.bzl: =m):
#  sg (CHR_DEV_SG; vendor: runtime-PM reference while open), icc-clk
#  (INTERCONNECT_CLK; vendor: num_nodes = registered nodes), qcom_smd
#  (RPMSG_QCOM_SMD; needs the ported smem), msm_qmp (MSM_QMP, vendor only).
obj-m += sg.o icc-clk.o qcom_smd.o msm_qmp.o
ccflags-y += -I$(src)/include -I$(srctree) -I$(src)/../qcom-smem/include -I$(src)/../qcom-ipc-logging/include
ccflags-y += -DCONFIG_RPMSG_QCOM_SMD_MODULE=1 -DCONFIG_MSM_QMP_MODULE=1
ccflags-y += -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-smem/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
