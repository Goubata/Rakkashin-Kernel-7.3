# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom_rpmh.o cmd-db.o
qcom_rpmh-y := rpmh.o rpmh-rsc.o
ccflags-y += -I$(src)/include -I$(src)
ccflags-y += -DCONFIG_QCOM_RPMH_MODULE=1 -DCONFIG_QCOM_COMMAND_DB_MODULE=1 -DCONFIG_QCOM_CRM_V2_MODULE=1 -DCONFIG_QCOM_RPMH_TIMEOUT=5000
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-clock-core/Module.symvers
