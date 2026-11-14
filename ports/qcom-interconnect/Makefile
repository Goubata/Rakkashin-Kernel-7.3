# SPDX-License-Identifier: GPL-2.0-only
obj-m += icc-bcm-voter.o icc-rpmh.o icc-debug.o qnoc-qos.o qnoc-alor.o
icc-bcm-voter-y := bcm-voter.o
qnoc-alor-y := alor.o
ccflags-y += -I$(src)/include -I$(src) -I$(src)/../qcom-clock-core/include -I$(src)/../qcom-socinfo/include -I$(srctree)
ccflags-y += -DCONFIG_INTERCONNECT_QCOM_DEBUG_MODULE=1 -DCONFIG_QCOM_RPMH_MODULE=1 -DCONFIG_QCOM_COMMAND_DB_MODULE=1 -DCONFIG_QCOM_CRM_V2_MODULE=1 -DCONFIG_QCOM_SOCINFO_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-socinfo/Module.symvers

# All five vendor buckets must precede the native three-bucket header guard.
ccflags-y += -include $(src)/include/dt-bindings/interconnect/qcom,icc.h
