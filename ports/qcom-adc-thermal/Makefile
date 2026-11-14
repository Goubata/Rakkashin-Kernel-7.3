# SPDX-License-Identifier: GPL-2.0-only
obj-m += qcom-vadc-common.o qcom-spmi-adc5-gen3.o qcom-spmi-temp-alarm.o
ccflags-y += -I$(src)/include -I$(src)/../qcom-ipc-logging/include -I$(srctree)
ccflags-y += -include $(src)/include/linux/iio/adc/qcom-vadc-common.h -include $(src)/include/dt-bindings/iio/qcom,spmi-vadc.h
ccflags-y += -DCONFIG_QCOM_SPMI_ADC5_GEN3_MODULE=1 -DCONFIG_QCOM_VADC_COMMON_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers
