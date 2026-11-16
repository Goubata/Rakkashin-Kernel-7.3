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
