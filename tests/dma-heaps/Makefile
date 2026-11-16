# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-gunyah-stub.o
# same include/config view as ports/gunyah-core so exported CRCs match
ccflags-y += -I$(src)/../../ports/gunyah-core/include -I$(src)/../../ports/gunyah/include -I$(src)/../../ports/gunyah-rm/include
ccflags-y += -DCONFIG_GUNYAH_MODULE=1 -DCONFIG_GUNYAH_HYPERCALLS_MODULE=1 -DCONFIG_GUNYAH_PLATFORM_HOOKS_MODULE=1 -DCONFIG_GUNYAH_IRQFD_MODULE=1 -DCONFIG_GUNYAH_IOEVENTFD_MODULE=1 -DCONFIG_GUNYAH_QCOM_TRUSTED_VM_MODULE=1
