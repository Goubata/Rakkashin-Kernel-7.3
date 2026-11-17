# SPDX-License-Identifier: GPL-2.0-only
# QTI NXP SN2xx NFC driver (clo vendor/nxp/opensource/driver nfc-driver.lnx.16.0.r1-rel),
# canoe copts from define_modules.bzl; phone: nxp-nci.ko, DT compatible qcom,sn-nci
obj-m += nxp-nci.o
nxp-nci-y := nfc/ese_cold_reset.o nfc/common.o nfc/common_nxp.o nfc/common_qcom.o nfc/i2c_drv.o
ccflags-y += -DCONFIG_NXP_NFC_I2C=1 -DCONFIG_NFC_BOB1 -DNFC_SECURE_PERIPHERAL_ENABLED
ccflags-y += -I$(src)/include/uapi/linux/nfc -I$(src)/../securemsm/include/linux -I$(src)/../securemsm/smcinvoke
ccflags-y += -I$(src)/../qcom-ipc-logging/include -I$(src)/../pinctrl-alor-vendor-core/include -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../securemsm/Module.symvers $(src)/../qcom-ipc-logging/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
