# SPDX-License-Identifier: GPL-2.0-only
obj-m += gh_arm_drv.o gh_dbl.o gh_msgq.o
gh_arm_drv-y := arm64/gh_arm.o arm64/irq.o arm64/reset.o
ccflags-y += -I$(src)/include -DCONFIG_GH_ARM64_DRV_MODULE=1 -DCONFIG_GH_DBL_MODULE=1 -DCONFIG_GH_MSGQ_MODULE=1 -DCONFIG_GH_RM_DRV_MODULE=1
