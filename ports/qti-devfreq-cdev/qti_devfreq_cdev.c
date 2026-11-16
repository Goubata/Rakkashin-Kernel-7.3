// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 * Native 7.3 lifetime, table validation and acknowledged QoS state port.
 */
#define pr_fmt(fmt) "%s:%s " fmt, KBUILD_MODNAME, __func__

#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/pm_qos.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define DEVFREQ_CDEV_NAME "gpu"
#define DEVFREQ_CDEV "qcom-devfreq-cdev"

struct devfreq_cdev_device {
	struct mutex lock;
	struct device *dev;
	struct device *gpu_dev;		/* referenced for the binding lifetime */
	unsigned long *freq_table;	/* kHz, state0 = highest OPP */
	unsigned int cur_state;
	unsigned int max_state;
	bool stopping;
	struct dev_pm_qos_request qos_max_freq_req;
	struct thermal_cooling_device *cdev;
};

static int devfreq_cdev_set_state(struct thermal_cooling_device *cdev,
				  unsigned long state)
{
	struct devfreq_cdev_device *d = cdev->devdata;
	int ret = 0;

	mutex_lock(&d->lock);
	if (d->stopping) {
		ret = -ENODEV;
		goto out;
	}
	if (state > d->max_state) {
		ret = -EINVAL;
		goto out;
	}
	if (state == d->cur_state)
		goto out;
	dev_dbg(d->dev, "cdev:%s Limit:%lu\n", cdev->type, d->freq_table[state]);
	ret = dev_pm_qos_update_request(&d->qos_max_freq_req, d->freq_table[state]);
	if (ret < 0) {
		dev_err(d->dev, "Error placing qos request:%lu. cdev:%s err:%d\n",
			d->freq_table[state], cdev->type, ret);
		goto out;
	}
	/* 0 means the aggregated constraint did not change; still acknowledged. */
	d->cur_state = state;
	ret = 0;
out:
	mutex_unlock(&d->lock);
	return ret;
}

static int devfreq_cdev_get_state(struct thermal_cooling_device *cdev,
				  unsigned long *state)
{
	struct devfreq_cdev_device *d = cdev->devdata;

	mutex_lock(&d->lock);
	*state = d->cur_state;
	mutex_unlock(&d->lock);
	return 0;
}

static int devfreq_cdev_get_max_state(struct thermal_cooling_device *cdev,
				      unsigned long *state)
{
	struct devfreq_cdev_device *d = cdev->devdata;

	*state = d->max_state;
	return 0;
}

static const struct thermal_cooling_device_ops devfreq_cdev_ops = {
	.set_cur_state = devfreq_cdev_set_state,
	.get_cur_state = devfreq_cdev_get_state,
	.get_max_state = devfreq_cdev_get_max_state,
};

static void devfreq_cdev_put_gpu(void *data)
{
	put_device(data);
}

/* Runs after the managed cooling device is unregistered (callbacks drained). */
static void devfreq_cdev_remove_qos(void *data)
{
	struct devfreq_cdev_device *d = data;

	mutex_lock(&d->lock);
	d->stopping = true;
	mutex_unlock(&d->lock);
	dev_pm_qos_remove_request(&d->qos_max_freq_req);
}

static int devfreq_cdev_read_table(struct devfreq_cdev_device *d)
{
	unsigned long freq = ULONG_MAX;
	struct dev_pm_opp *opp;
	int count, i;

	count = dev_pm_opp_get_opp_count(d->gpu_dev);
	if (count < 0)
		return count;
	if (!count)
		return -ENODATA;
	d->freq_table = devm_kcalloc(d->dev, count, sizeof(*d->freq_table), GFP_KERNEL);
	if (!d->freq_table)
		return -ENOMEM;
	for (i = 0; i < count; i++) {
		opp = dev_pm_opp_find_freq_floor(d->gpu_dev, &freq);
		if (IS_ERR(opp))
			return PTR_ERR(opp);
		dev_pm_opp_put(opp);
		d->freq_table[i] = DIV_ROUND_UP(freq, 1000); /* Hz to kHz */
		if (!d->freq_table[i] || d->freq_table[i] > S32_MAX)
			return -ERANGE;
		dev_dbg(d->dev, "%d. freq table:%lu\n", i, d->freq_table[i]);
		if (!freq)
			return -ERANGE;
		freq--;
	}
	d->max_state = count - 1;
	return 0;
}

static int devfreq_cdev_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct devfreq_cdev_device *d;
	struct platform_device *gpu_pdev;
	struct device_node *gpu_np;
	struct device_link *link;
	int ret;

	d = devm_kzalloc(dev, sizeof(*d), GFP_KERNEL);
	if (!d)
		return -ENOMEM;
	mutex_init(&d->lock);
	d->dev = dev;

	gpu_np = of_parse_phandle(dev->of_node, "qcom,devfreq", 0);
	if (!gpu_np)
		return dev_err_probe(dev, -EINVAL, "missing qcom,devfreq phandle\n");
	gpu_pdev = of_find_device_by_node(gpu_np);
	of_node_put(gpu_np);
	if (!gpu_pdev)
		return dev_err_probe(dev, -EPROBE_DEFER, "GPU device not created yet\n");
	ret = devm_add_action_or_reset(dev, devfreq_cdev_put_gpu, &gpu_pdev->dev);
	if (ret)
		return ret;
	d->gpu_dev = &gpu_pdev->dev;

	/* Managed link: consumer is unbound before the GPU driver goes away. */
	link = device_link_add(dev, d->gpu_dev, DL_FLAG_AUTOPROBE_CONSUMER);
	if (!link)
		return dev_err_probe(dev, -ENODEV, "add gpu device_link fail\n");
	if (link->status == DL_STATE_DORMANT || !READ_ONCE(d->gpu_dev->driver))
		return dev_err_probe(dev, -EPROBE_DEFER, "kgsl not probed yet\n");

	ret = devfreq_cdev_read_table(d);
	if (ret)
		return dev_err_probe(dev, ret, "invalid GPU OPP table\n");

	ret = dev_pm_qos_add_request(d->gpu_dev, &d->qos_max_freq_req,
				     DEV_PM_QOS_MAX_FREQUENCY,
				     PM_QOS_MAX_FREQUENCY_DEFAULT_VALUE);
	if (ret < 0)
		return ret;
	ret = devm_add_action_or_reset(dev, devfreq_cdev_remove_qos, d);
	if (ret)
		return ret;

	/* Factory registers by type "gpu" without an OF node; keep that ABI. */
	d->cdev = devm_thermal_cooling_device_register(dev, DEVFREQ_CDEV_NAME,
						       d, &devfreq_cdev_ops);
	if (IS_ERR(d->cdev))
		return dev_err_probe(dev, PTR_ERR(d->cdev), "Cdev register failed for gpu\n");

	platform_set_drvdata(pdev, d);
	return 0;
}

static const struct of_device_id devfreq_cdev_match[] = {
	{ .compatible = "qcom,devfreq-cdev" },
	{}
};
MODULE_DEVICE_TABLE(of, devfreq_cdev_match);

static struct platform_driver devfreq_cdev_driver = {
	.probe = devfreq_cdev_probe,
	.driver = {
		.name = DEVFREQ_CDEV,
		.of_match_table = devfreq_cdev_match,
	},
};
module_platform_driver(devfreq_cdev_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. devfreq cooling device driver");
MODULE_LICENSE("GPL");
