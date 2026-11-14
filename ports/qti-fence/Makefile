# SPDX-License-Identifier: GPL-2.0-only
obj-m += synx-driver.o ipclite.o msm_hw_fence.o
synx-driver-y := synx/msm/synx/synx.o synx/msm/synx/synx_util.o synx/msm/synx/synx_debugfs.o synx/msm/synx/synx_global.o synx/msm/synx/synx_compat.o synx/msm/synx/synx_test_ioctl.o synx/msm/synx/synx_ioctl.o
synx-driver-$(CONFIG_DEBUG_FS) += synx/msm/synx/synx_debugfs_util.o
ipclite-y := synx/msm/synx/ipclite.o
msm_hw_fence-y := hw_fence/src/msm_hw_fence.o hw_fence/src/hw_fence_drv_priv.o hw_fence/src/hw_fence_drv_utils.o hw_fence/src/hw_fence_drv_debug.o hw_fence/src/hw_fence_drv_ipc.o hw_fence/src/msm_hw_fence_synx_translation.o hw_fence/src/hw_fence_drv_interop.o
msm_hw_fence-$(CONFIG_DEBUG_FS) += hw_fence/src/hw_fence_ioctl.o
ccflags-y += -I$(src)/include -I$(src)/hw_fence/include -I$(src)/synx/msm/synx -I$(src)/synx/include/uapi/synx/media
ccflags-y += -DCONFIG_QTI_HW_FENCE_MODULE=1 -DCONFIG_QTI_HW_FENCE_USE_SYNX=1 -DCONFIG_QTI_ENABLE_HW_FENCE_DEFAULT=1
ccflags-y += -DCONFIG_GH_RM_DRV_MODULE=1 -DCONFIG_GH_DBL_MODULE=1 -DCONFIG_QCOM_RPROC_COMMON_MODULE=1 -DCONFIG_QCOM_SECURE_BUFFER_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../secure-buffer/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-hwspinlock/Module.symvers

ccflags-y += -include $(src)/avr-fence-compat.h

ccflags-y += -include $(src)/avr-platform-deps.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-rproc-common/Module.symvers $(src)/../gunyah/Module.symvers
