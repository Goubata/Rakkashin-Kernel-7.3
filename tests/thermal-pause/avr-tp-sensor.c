// SPDX-License-Identifier: GPL-2.0-only
/* Private recorded temperature source for /avr-tp-sensor; poke via the temp param. */
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>

static struct thermal_zone_device *tz;
static struct platform_device *pdev;
static int temp_mc = 30000;

static int set_temp(const char *val, const struct kernel_param *kp)
{
	int ret = param_set_int(val, kp);

	if (!ret && tz) {
		int i;

		for (i = 0; i < 3; i++)
			thermal_zone_device_update(tz, THERMAL_EVENT_UNSPECIFIED);
	}
	return ret;
}
static const struct kernel_param_ops temp_ops = { .set = set_temp, .get = param_get_int };
module_param_cb(temp, &temp_ops, &temp_mc, 0644);

static int get_temp(struct thermal_zone_device *z, int *t)
{
	*t = READ_ONCE(temp_mc);
	return 0;
}
static const struct thermal_zone_device_ops ops = { .get_temp = get_temp };

static int __init tp_init(void)
{
	struct device_node *np = of_find_node_by_path("/avr-tp-sensor");
	int ret;

	if (!np)
		return -ENODEV;
	pdev = platform_device_alloc("avr-tp-sensor", PLATFORM_DEVID_NONE);
	if (!pdev) {
		of_node_put(np);
		return -ENOMEM;
	}
	device_set_node(&pdev->dev, of_fwnode_handle(np));
	ret = platform_device_add(pdev);
	if (ret) {
		platform_device_put(pdev);
		return ret;
	}
	tz = devm_thermal_of_zone_register(&pdev->dev, 0, NULL, &ops);
	if (IS_ERR(tz)) {
		ret = PTR_ERR(tz);
		platform_device_unregister(pdev);
		return ret;
	}
	return 0;
}
module_init(tp_init);

static void __exit tp_exit(void)
{
	platform_device_unregister(pdev);
}
module_exit(tp_exit);
MODULE_LICENSE("GPL");
