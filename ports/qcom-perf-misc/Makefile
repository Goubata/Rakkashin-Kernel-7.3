# SPDX-License-Identifier: GPL-2.0-only
# canoe_perf.bzl =m: ARM_SMMU_PMU, QCOM_LLCC_PMU, QCOM_PMU_LIB, MSM_PERFORMANCE,
# QCOM_CPU_VENDOR_HOOKS, GH_VCPU_MGR.
obj-m += arm_smmu_pmu.o qcom_llcc_pmu.o qcom-pmu-lib.o msm_performance.o \
	qcom_cpu_vendor_hooks.o gh_vcpu_mgr.o
qcom-pmu-lib-y := pmu_lib.o
ccflags-y += -I$(src)/include -I$(src) -I$(srctree) -I$(src)/../qcom-minidump \
	-I$(src)/../qcom-minidump/include -I$(src)/../qcom-socinfo/include \
	-I$(src)/../qcom-watchdog/include -I$(src)/../gunyah-rm/include -I$(src)/../gunyah/include \
	-I$(src)/../gunyah-core/include -I$(src)/../sched-walt/include \
	-I$(src)/../qcom-cpucp-scmi/include
ccflags-y += -DCONFIG_QCOM_PMU_LIB_MODULE=1 -DCONFIG_QCOM_LLCC_PMU_MODULE=1 \
	-DCONFIG_MSM_PERFORMANCE_MODULE=1 -DCONFIG_SCHED_WALT_MODULE=1 -DCONFIG_SCHED_WALT_NR_CPUS=8 \
	-DCONFIG_QTI_SCMI_VENDOR_PROTOCOL_MODULE=1 -DCONFIG_QTI_QCOM_SCMI_CLIENT_MODULE=1 \
	-DCONFIG_GH_VCPU_MGR_MODULE=1 -DCONFIG_GH_RM_DRV_MODULE=1 -DCONFIG_QCOM_MINIDUMP_MODULE=1 \
	-DCONFIG_QCOM_WDT_CORE_MODULE=1 -DCONFIG_QCOM_CPU_VENDOR_HOOKS_MODULE=1 -DCONFIG_QCOM_DEBUG_SYMBOL_MODULE=1
ccflags-y += -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../sched-walt/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers \
	$(src)/../gunyah-rm/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-core/Module.symvers \
	$(src)/../qcom-minidump/Module.symvers $(src)/../qcom-socinfo/Module.symvers \
	$(src)/../qcom-watchdog/Module.symvers
