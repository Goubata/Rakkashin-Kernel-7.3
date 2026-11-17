# SPDX-License-Identifier: GPL-2.0-only
# QTI ST54 NFC driver (clo vendor/st/opensource/driver nfc-st-driver.lnx.16.0.r1-rel),
# canoe copts from define_modules.bzl; phone: stm_nfc_i2c.ko
obj-m += stm_nfc_i2c.o
stm_nfc_i2c-y := nfc/st21nfc.o
ccflags-y += -DCONFIG_NFC_BOB1 -DNFC_SECURE_PERIPHERAL_ENABLED -Wno-misleading-indentation
ccflags-y += -I$(src)/include/uapi/linux/nfc -I$(src)/../securemsm/include/linux -I$(src)/../securemsm/smcinvoke
ccflags-y += -I$(src)/../pinctrl-alor-vendor-core/include -Wno-missing-prototypes
KBUILD_EXTRA_SYMBOLS += $(src)/../securemsm/Module.symvers $(src)/../pinctrl-alor-vendor-core/Module.symvers
ccflags-y += -include $(src)/avr-canoe-vendor-config.h
