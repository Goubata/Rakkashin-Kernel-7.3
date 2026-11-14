# SPDX-License-Identifier: GPL-2.0-only
obj-m += gh_rm_drv.o
gh_rm_drv-y := gh_rm_core.o gh_rm_iface.o
ccflags-y += -I$(src)/include -I$(src)/../gunyah/include -I$(src)/../secure-buffer/include
ccflags-y += -DCONFIG_GH_RM_DRV_MODULE=1 -DCONFIG_GH_ARM64_DRV_MODULE=1 -DCONFIG_GH_DBL_MODULE=1 -DCONFIG_GH_MSGQ_MODULE=1 -DCONFIG_GUNYAH_MODULE=1 -DCONFIG_QCOM_SECURE_BUFFER_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../gunyah/Module.symvers $(src)/../secure-buffer/Module.symvers
ccflags-y += -include $(src)/avr-rm-deps.h
