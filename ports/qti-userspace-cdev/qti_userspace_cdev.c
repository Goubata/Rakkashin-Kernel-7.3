// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Native 7.3 child-node registration and OF reference ownership port.
 */
#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define USERSPACE_CDEV_DRIVER		"userspace-cdev"

/* Level is only stored: the vendor HAL reads it back through thermal sysfs. */
struct userspace_cdev {
	struct device_node		*np;	/* referenced */
	char				cdev_name[THERMAL_NAME_LENGTH];
	struct thermal_cooling_device	*cdev;
	unsigned int			cur_level;
	unsigned int			max_level;
};

static int userspace_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct userspace_cdev *u = cdev->devdata;

	*state = u->max_level;
	return 0;
}

static int userspace_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct userspace_cdev *u = cdev->devdata;

	*state = READ_ONCE(u->cur_level);
	return 0;
}

static int userspace_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct userspace_cdev *u = cdev->devdata;

	if (state > u->max_level)
		return -EINVAL;
	WRITE_ONCE(u->cur_level, state);
	return 0;
}

static const struct thermal_cooling_device_ops userspace_cdev_ops = {
	.get_max_state = userspace_get_max_state,
	.get_cur_state = userspace_get_cur_state,
	.set_cur_state = userspace_set_cur_state,
};

static void userspace_cdev_release(void *data)
{
	struct userspace_cdev *u = data;

	if (u->cdev)
		thermal_cooling_device_unregister(u->cdev);
	u->cdev = NULL;
	of_node_put(u->np);
}

static int userspace_device_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *child;
	struct userspace_cdev *inst;
	int ret, idx = 0, cnt;

	cnt = of_get_available_child_count(dev->of_node);
	if (!cnt)
		return dev_err_probe(dev, -ENODEV, "No child node to process\n");
	inst = devm_kcalloc(dev, cnt, sizeof(*inst), GFP_KERNEL);
	if (!inst)
		return -ENOMEM;

	for_each_available_child_of_node(dev->of_node, child) {
		struct userspace_cdev *u;

		if (idx >= cnt) {
			of_node_put(child);
			break;
		}
		u = &inst[idx++];
		ret = of_property_read_u32(child, "qcom,max-level", &u->max_level);
		if (ret) {
			dev_err(dev, "error reading qcom,max-level in %pOF: %d\n", child, ret);
			of_node_put(child);
			return ret;
		}
		u->np = of_node_get(child);
		strscpy(u->cdev_name, child->name, THERMAL_NAME_LENGTH);
		/* 7.3's devm OF helper binds dev->of_node; the factory binds each child. */
		u->cdev = thermal_of_cooling_device_register(child, 0, u->cdev_name, u,
							     &userspace_cdev_ops);
		if (IS_ERR(u->cdev)) {
			ret = PTR_ERR(u->cdev);
			u->cdev = NULL;
			of_node_put(u->np);
			of_node_put(child);
			return dev_err_probe(dev, ret, "Error registering cdev:%s\n", u->cdev_name);
		}
		ret = devm_add_action_or_reset(dev, userspace_cdev_release, u);
		if (ret) {
			of_node_put(child);
			return ret;
		}
		dev_info(dev, "cdev:%s lvl:%u registered\n", u->cdev_name, u->max_level);
	}
	platform_set_drvdata(pdev, inst);
	return 0;
}

static const struct of_device_id userspace_device_match[] = {
	{.compatible = "qcom,userspace-cooling-devices"},
	{}
};
MODULE_DEVICE_TABLE(of, userspace_device_match);

static struct platform_driver userspace_cdev_driver = {
	.probe = userspace_device_probe,
	.driver = {
		.name = USERSPACE_CDEV_DRIVER,
		.of_match_table = userspace_device_match,
	},
};
module_platform_driver(userspace_cdev_driver);

MODULE_LICENSE("GPL");
