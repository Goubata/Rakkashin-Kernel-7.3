# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-adc-thermal-test.o avr-adc-driver-test.o avr-temp-alarm-test.o
ccflags-y += -I$(src)/../../ports/qcom-adc-thermal/include -I$(src)/../../ports/qcom-ipc-logging/include -I$(srctree)
ccflags-y += -include $(src)/../../ports/qcom-adc-thermal/include/linux/iio/adc/qcom-vadc-common.h
ccflags-y += -DCONFIG_QCOM_SPMI_ADC5_GEN3_MODULE=1 -DCONFIG_QCOM_VADC_COMMON_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-adc-thermal/Module.symvers
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/qcom-ipc-logging/Module.symvers
ccflags-y += -include $(src)/../../ports/qcom-adc-thermal/include/dt-bindings/iio/qcom,spmi-vadc.h
