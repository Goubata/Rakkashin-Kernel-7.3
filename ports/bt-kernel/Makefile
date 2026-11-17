# SPDX-License-Identifier: GPL-2.0-only
# vendor/qcom/opensource/bt-kernel, canoe (target.bzl define_canoe), the modules the
# phone ships (thqspi_proto is listed for canoe but not in vendor_dlkm).
obj-m += btpower.o btfm_slim_codec.o btfmcodec.o radio-i2c-rtc6226-qca.o bt_fm_swr.o spi_cnss_proto.o
btpower-y := pwr/btpower.o
btfm_slim_codec-y := slimbus/btfm_slim.o slimbus/btfm_slim_slave.o slimbus/btfm_slim_hw_interface.o
btfmcodec-y := btfmcodec/btfm_codec.o btfmcodec/btfm_codec_btadv_interface.o \
	btfmcodec/btfm_codec_hw_interface.o btfmcodec/btfm_codec_interface.o
radio-i2c-rtc6226-qca-y := rtc6226/radio-rtc6226-common.o rtc6226/radio-rtc6226-i2c.o
bt_fm_swr-y := soundwire/btfm_swr.o soundwire/btfm_swr_slave.o soundwire/btfm_swr_hw_interface.o
spi_cnss_proto-y := spi/spi_cnss_proto.o
# canoe config_options + each module's config_opt (local_defines)
ccflags-y += -DCONFIG_MSM_BT_POWER=1 -DCONFIG_I2C_RTC6226_QCA=1 -DCONFIG_SLIM_BTFM_CODEC=1 \
	-DCONFIG_BTFM_CODEC=1 -DCONFIG_BTFM_SWR=1 -DCONFIG_FMD_ENABLE=1 -DCONFIG_SPI_CNSS_PROTO=1 \
	-DCONFIG_THQSPI_PROTO=1
ccflags-y += -I$(src)/include -I$(src)/btfmcodec/include -I$(src)/spi/include -I$(src)/slimbus -I$(src)/soundwire \
	-I$(src)/../wlan-platform/inc -I$(src)/../qcom-slimbus/include \
	-I$(src)/../audio-kernel/include -I$(src)/../audio-kernel -I$(src)/../audio-kernel/include/uapi/audio \
	-I$(src)/../qcom-ipc-logging/include -I$(src)/../qcom-rproc-common/include \
	-I$(src)/../pinctrl-alor-vendor-core/include -Wno-missing-prototypes -Wno-macro-redefined
ccflags-y += -include $(src)/avr-canoe-vendor-config.h -include $(src)/../audio-kernel/avr-audio-compat.h \
	-include $(src)/../qcom-slimbus/include/linux/slimbus_vendor.h
KBUILD_EXTRA_SYMBOLS += $(src)/../wlan-platform/Module.symvers $(src)/../qcom-slimbus/Module.symvers \
	$(src)/../audio-kernel/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers \
	$(src)/../qcom-rproc-common/Module.symvers $(src)/../qcom-remoteproc-services/Module.symvers \
	$(src)/../pinctrl-alor-vendor-core/Module.symvers $(src)/../qcom-wlan-deps/Module.symvers
