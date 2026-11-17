/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AVR 7.3: devm_gpio_request() was removed; the remaining devm_gpio_request_one()
 * always sets a direction. The vendor drivers expect a plain request (direction
 * set later by the driver), so keep 6.12 semantics: request + devm release.
 */
#ifndef AVR_GPIO_COMPAT_H
#define AVR_GPIO_COMPAT_H
#include <linux/device.h>
#include <linux/gpio.h>

static void avr_devm_gpio_release(void *gpio)
{
	gpio_free((unsigned int)(unsigned long)gpio);
}

static inline int avr_devm_gpio_request(struct device *dev, unsigned int gpio, const char *label)
{
	int rc = gpio_request(gpio, label);

	if (rc)
		return rc;
	return devm_add_action_or_reset(dev, avr_devm_gpio_release, (void *)(unsigned long)gpio);
}
#define devm_gpio_request avr_devm_gpio_request
#endif
