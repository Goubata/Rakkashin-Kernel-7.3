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
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers $(src)/../qcom-leds/Module.symvers $(src)/../mm-drivers/Module.symvers $(src)/../qti-fence/Module.symvers $(src)/../securemsm/Module.symvers $(src)/../qcom-charging/Module.symvers $(src)/../qcom-cpucp-scmi/Module.symvers $(src)/../qcom-glink-transport/Module.symvers $(src)/../sched-walt/Module.symvers $(src)/../qcom-panel-event/Module.symvers $(src)/../qcom-regulators/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../gunyah-misc/Module.symvers
