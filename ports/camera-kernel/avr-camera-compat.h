/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AVR 7.3 port: android16-6.12 APIs used by the canoe camera-kernel.
 * - struct gpio (legacy <linux/gpio.h> array descriptor, gone in 7.x): same layout
 * - I3C: struct i3c_priv_xfer -> struct i3c_xfer (same fields used: rnw, len,
 *   data.in/out, err); i3c_device_do_priv_xfers() == SDR mode transfers
 */
#ifndef AVR_CAMERA_COMPAT_H
#define AVR_CAMERA_COMPAT_H
#include <linux/i3c/device.h>
#include <uapi/linux/dma-buf.h>

struct gpio {
	unsigned	gpio;
	unsigned long	flags;
	const char	*label;
};

#define i3c_priv_xfer i3c_xfer
static inline int i3c_device_do_priv_xfers(struct i3c_device *dev, struct i3c_xfer *xfers,
					   int nxfers)
{
	return i3c_device_do_xfers(dev, xfers, nxfers, I3C_SDR);
}
#endif
