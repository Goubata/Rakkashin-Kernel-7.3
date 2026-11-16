# SPDX-License-Identifier: GPL-2.0-only
# Qualcomm TEE transport (canoe: CONFIG_QCOM_SI_CORE=m, _ADCI=y, _WQ=y; no MEM_FFA/DOORBELL)
# and qseecom_proxy, from qcom-soc (android16-6.12 AVR tree).
obj-m += qseecom_proxy.o si_core_module.o
si_core_module-y := si_core/qcom_scm_invoke.o si_core/si_core.o si_core/si_core_async.o \
	si_core/si_core_helpers.o si_core/xts/mem-object.o si_core/si_core_adci.o si_core/si_core_wq.o

ccflags-y += -I$(src)/include -I$(src)/si_core -I$(src)/si_core/xts -I$(src)/../qcom-dma-heaps/include \
	-I$(src)/../secure-buffer/include -I$(src)/../gunyah-rm/include -I$(src)/../gunyah/include
# vendor code is built without W=1 by kleaf; 7.3 promotes this to -Werror
ccflags-y += -Wno-missing-prototypes
ccflags-y += -DCONFIG_QCOM_SI_CORE_MODULE=1 -DCONFIG_QCOM_SI_CORE_ADCI=1 -DCONFIG_QCOM_SI_CORE_WQ=1 \
	-DCONFIG_QSEECOM_PROXY_MODULE=1
CFLAGS_si_core.o += -I$(src)/si_core
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-dma-heaps/Module.symvers $(src)/../secure-buffer/Module.symvers
