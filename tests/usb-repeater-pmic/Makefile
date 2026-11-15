# SPDX-License-Identifier: GPL-2.0-only
obj-m += avr-usb-repeater-pmic-test.o
ccflags-y += -I$(src)/../../ports/usb-repeater/include -DCONFIG_USB_REPEATER_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../../ports/usb-repeater/Module.symvers
