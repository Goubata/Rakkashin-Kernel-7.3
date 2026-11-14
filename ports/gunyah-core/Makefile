# SPDX-License-Identifier: GPL-2.0-only
obj-m += gunyah_hypercall.o gunyah.o gunyah_platform_hooks.o gunyah_rsc_mgr.o gunyah_vcpu.o gunyah_irqfd.o gunyah_ioeventfd.o gunyah_qtvm.o
gunyah_rsc_mgr-y := rsc_mgr.o rsc_mgr_rpc.o vm_mgr.o vm_mgr_mem.o
# Preserve the same optional CMA path as the target kernel configuration.
gunyah_rsc_mgr-$(CONFIG_DMA_CMA) += vm_mgr_cma_mem.o
ccflags-y += -I$(src)/include -I$(src)/../gunyah/include -I$(src)/../gunyah-rm/include
ccflags-y += -DCONFIG_GUNYAH_MODULE=1 -DCONFIG_GUNYAH_HYPERCALLS_MODULE=1 -DCONFIG_GUNYAH_PLATFORM_HOOKS_MODULE=1 -DCONFIG_GUNYAH_IRQFD_MODULE=1 -DCONFIG_GUNYAH_IOEVENTFD_MODULE=1 -DCONFIG_GUNYAH_QCOM_TRUSTED_VM_MODULE=1
