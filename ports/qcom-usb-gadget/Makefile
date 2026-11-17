# SPDX-License-Identifier: GPL-2.0-only
# qcom-soc 6.12 USB gadget functions (canoe_perf: USB_F_GSI/CDEV/CCID/QDSS=m) and the
# Motorola BLAN function moto_f_usbnet (motorola-kernel-modules drivers/moto_f_usbnet).
obj-m += usb_f_gsi.o usb_f_cdev.o usb_f_ccid.o usb_f_qdss.o moto_f_usbnet.o
usb_f_gsi-y := f_gsi.o rndis.o
usb_f_cdev-y := f_cdev.o
usb_f_ccid-y := f_ccid.o
usb_f_qdss-y := f_qdss.o u_qdss.o
moto_f_usbnet-y := f_usbnet.o
ccflags-y += -I$(src) -I$(src)/include -I$(src)/include/uapi -I$(srctree)/drivers/usb/gadget \
	-I$(srctree)/drivers/usb/gadget/function -I$(src)/../usb-repeater/include \
	-I$(src)/../qcom-ipc-logging/include -Wno-missing-prototypes
ccflags-y += -DCONFIG_USB_F_GSI_MODULE=1 -DCONFIG_USB_F_CDEV_MODULE=1 -DCONFIG_USB_F_CCID_MODULE=1 \
	-DCONFIG_USB_F_QDSS_MODULE=1 -DCONFIG_IPC_LOGGING_MODULE=1 -DCONFIG_USB_DWC3_MSM_MODULE=1
KBUILD_EXTRA_SYMBOLS += $(src)/../qcom-ipc-logging/Module.symvers $(src)/../qcom-usb/Module.symvers
