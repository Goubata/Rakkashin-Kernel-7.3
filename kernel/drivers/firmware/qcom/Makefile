# SPDX-License-Identifier: GPL-2.0
#
# Makefile for the linux kernel.
#

obj-$(CONFIG_QCOM_SCM)		+= qcom-scm.o
qcom-scm-objs += qcom_scm.o qcom_scm-smc.o qcom_scm-legacy.o
CFLAGS_qcom_scm-smc.o := -I$(src)
# SHM Bridge calls SCM, and SCM allocates from TZMEM. Keep both in one
# module in this mode so their real exports do not form a load-time cycle.
ifeq ($(CONFIG_QCOM_TZMEM_MODE_SHMBRIDGE),y)
qcom-scm-objs += qcom_tzmem.o
else
obj-$(CONFIG_QCOM_TZMEM)	+= qcom_tzmem.o
endif
obj-$(CONFIG_QCOM_QSEECOM)	+= qcom_qseecom.o
obj-$(CONFIG_QCOM_QSEECOM_UEFISECAPP) += qcom_qseecom_uefisecapp.o
obj-$(CONFIG_QCOM_PAS)		+= qcom_pas.o
obj-$(CONFIG_QCOM_PAS_TEE)	+= qcom_pas_tee.o
