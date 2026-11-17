# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom-vadc-common.o qcom-spmi-adc5-gen3.o qcom-spmi-temp-alarm.o
ccflags-y += -I$(src)/include -I$(src)/../qcom-ipc-logging/include -I$(srctree)
ccflags-y += -include $(src)/include/linux/iio/adc/qcom-vadc-common.h -include $(src)/include/dt-bindings/iio/qcom,spmi-vadc.h
ccflags-y += -DCONFIG_QCOM_SPMI_ADC5_GEN3_MODULE=1 -DCONFIG_QCOM_VADC_COMMON_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers
# vendor CONFIG parity (scripts/apply_vendor_config_parity.py)
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-pas-alor/Module.symvers $(src)/../qcom-rpmh/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers $(src)/../qcom-minidump/Module.symvers $(src)/../qcom-debug-diag/Module.symvers $(src)/../qcom-perf-misc/Module.symvers $(src)/../qcom-dma-heaps/Module.symvers $(src)/../qcom-sps/Module.symvers $(src)/../qcom-spmi/Module.symvers $(src)/../qcom-display-deps/Module.symvers $(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-smem/Module.symvers $(src)/../qcom-socinfo/Module.symvers $(src)/../qcom-iommu-util/Module.symvers $(src)/../secure-buffer/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers $(src)/../qcom-eom/Module.symvers $(src)/../qcom-pm-telemetry/Module.symvers $(src)/../gunyah/Module.symvers $(src)/../gunyah-rm/Module.symvers $(src)/../gunyah-core/Module.symvers $(src)/../qcom-llcc/Module.symvers $(src)/../qcom-clock-core/Module.symvers $(src)/../qcom-dcvs/Module.symvers $(src)/../msm-sysstats/Module.symvers $(src)/../qcom-usb/Module.symvers
