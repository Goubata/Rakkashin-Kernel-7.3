# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom-dcvs.o dcvs_fp.o
qcom-dcvs-y := dcvs.o dcvs_epss.o dcvs_icc.o trace-dcvs.o
ccflags-y += -I$(src) -I$(src)/include -I$(src)/../qcom-clock-core/include
ccflags-y += -DCONFIG_QCOM_DCVS_MODULE=1 -DCONFIG_QCOM_DCVS_FP_MODULE=1 -DCONFIG_QCOM_RPMH_MODULE=1 -DCONFIG_QCOM_COMMAND_DB_MODULE=1 -DCONFIG_QCOM_CRM_V2_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-clock-core/Module.symvers
