// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 * Native 7.3 lifetime/ownership port; depletion and boot policy retained.
 */
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/thermal.h>
#include <linux/cpufreq.h>
#include <linux/pm_qos.h>
#include "drivers/thermal/thermal_core.h"

#define BCL_DRIVER_NAME "bcl_soc_peripheral"
#define RETRY_DELAY msecs_to_jiffies(1000)
#define BOOT_SOCD_THRESHOLD 97
/* Retry demand only: no per-device state or supply pointers are shared. */
static atomic_t bcl_waiting_supplies = ATOMIC_INIT(0);
static int boot_frequency = INT_MAX;
module_param(boot_frequency, int, 0400);

struct bcl_qos {
	struct freq_qos_request request;
	struct cpufreq_policy *policy;
};
struct bcl_device {
	struct device *dev;
	struct notifier_block psy_nb;
	struct work_struct soc_eval_work;
	struct delayed_work boot_work;
	struct mutex state_trans_lock;
	int trip_temp;
	bool irq_enabled;
	bool ready;
	bool stopping;
	bool notifier_registered;
	bool boot_limit_enabled;
	struct power_supply *batt_psy, *usb_psy;
	struct thermal_zone_device *tz_dev;
	struct bcl_qos *limits;
};

static void bcl_soc_clear_cpu_boot_limit(void *data)
{
	struct bcl_device *bcl = data;
	int cpu;

	for_each_possible_cpu(cpu) {
		struct bcl_qos *limit = &bcl->limits[cpu];

		if (!limit->policy)
			continue;
		if (freq_qos_request_active(&limit->request))
			freq_qos_remove_request(&limit->request);
		cpufreq_cpu_put(limit->policy);
		limit->policy = NULL;
	}
	bcl->boot_limit_enabled = false;
}

static int bcl_soc_trigger_cpu_boot_limit(struct bcl_device *bcl)
{
	struct cpufreq_policy *policy;
	int cpu, prev, ret = 0;

	cpus_read_lock();
	for_each_possible_cpu(cpu) {
		policy = cpufreq_cpu_get(cpu);
		if (!policy) {
			ret = -EPROBE_DEFER;
			goto fail;
		}
		/* Related CPUs share constraints: own one request/reference per policy. */
		for (prev = 0; prev < nr_cpu_ids; prev++)
			if (bcl->limits[prev].policy == policy)
				break;
		if (prev < nr_cpu_ids) {
			cpufreq_cpu_put(policy);
			continue;
		}
		ret = freq_qos_add_request(&policy->constraints, &bcl->limits[cpu].request,
					  FREQ_QOS_MAX, boot_frequency);
		if (ret < 0) {
			cpufreq_cpu_put(policy);
			goto fail;
		}
		bcl->limits[cpu].policy = policy;
	}
	cpus_read_unlock();
	bcl->boot_limit_enabled = true;
	return 0;
fail:
	cpus_read_unlock();
	bcl_soc_clear_cpu_boot_limit(bcl);
	return ret;
}

static int bcl_read_depletion(struct bcl_device *bcl, int *value)
{
	union power_supply_propval prop;
	int ret;

	if (!bcl->batt_psy)
		return -ENODEV;
	ret = power_supply_get_property(bcl->batt_psy, POWER_SUPPLY_PROP_PRESENT, &prop);
	if (ret)
		return ret;
	if (!prop.intval) {
		*value = 0; /* Preserve the factory absent-battery result. */
		return 0;
	}
	if (prop.intval != 1)
		return -ERANGE;
	ret = power_supply_get_property(bcl->batt_psy, POWER_SUPPLY_PROP_CAPACITY, &prop);
	if (ret)
		return ret;
	if (prop.intval < 0 || prop.intval > 100)
		return -ERANGE;
	*value = 100 - prop.intval;
	return 0;
}

static int bcl_read_soc(struct thermal_zone_device *tz, int *value)
{
	return bcl_read_depletion(thermal_zone_device_priv(tz), value);
}

static int soc_read_usb_charger_type(struct bcl_device *bcl, int *type)
{
	union power_supply_propval prop;
	int ret;

	if (!bcl->usb_psy)
		return -ENODEV;
	ret = power_supply_get_property(bcl->usb_psy, POWER_SUPPLY_PROP_USB_TYPE, &prop);
	if (ret)
		return ret;
	*type = prop.intval;
	return 0;
}

static int bcl_soc_get_trend(struct thermal_zone_device *tz,
			   const struct thermal_trip *trip, enum thermal_trend *trend)
{
	int temp;

	if (!tz || !trip || !trend)
		return -EINVAL;
	temp = READ_ONCE(tz->temperature);
	if (temp >= trip->temperature)
		*trend = THERMAL_TREND_RAISING;
	else if ((s64)temp <= (s64)trip->temperature - trip->hysteresis || !trip->hysteresis)
		*trend = THERMAL_TREND_DROPPING;
	else
		*trend = THERMAL_TREND_STABLE;
	return 0;
}

static int bcl_set_soc(struct thermal_zone_device *tz, int low, int high)
{
	struct bcl_device *bcl = thermal_zone_device_priv(tz);
	int ret = 0;

	if (high != INT_MAX && (high < 0 || high > 100))
		return -ERANGE;
	mutex_lock(&bcl->state_trans_lock);
	if (bcl->stopping) {
		ret = high == INT_MAX ? 0 : -ENODEV;
		goto out;
	}
	if (high != bcl->trip_temp) {
		bcl->trip_temp = high;
		bcl->irq_enabled = high != INT_MAX;
		if (bcl->irq_enabled && bcl->ready)
			schedule_work(&bcl->soc_eval_work);
	}
out:
	mutex_unlock(&bcl->state_trans_lock);
	return ret;
}

static void bcl_evaluate_soc(struct work_struct *work)
{
	struct bcl_device *bcl = container_of(work, struct bcl_device, soc_eval_work);
	int depletion;
	bool notify;

	if (bcl_read_depletion(bcl, &depletion))
		return;
	mutex_lock(&bcl->state_trans_lock);
	notify = bcl->ready && !bcl->stopping && bcl->irq_enabled &&
		 depletion >= bcl->trip_temp;
	mutex_unlock(&bcl->state_trans_lock);
	/* Never call back into thermal core while holding the driver mutex. */
	if (notify)
		thermal_zone_device_update(bcl->tz_dev, THERMAL_TRIP_VIOLATED);
}

static void bcl_boot_work(struct work_struct *work)
{
	struct bcl_device *bcl = container_of(to_delayed_work(work), struct bcl_device, boot_work);
	int depletion, type, soc_ret, usb_ret;

	soc_ret = bcl_read_depletion(bcl, &depletion);
	usb_ret = soc_read_usb_charger_type(bcl, &type);
	mutex_lock(&bcl->state_trans_lock);
	if (bcl->stopping || !bcl->boot_limit_enabled)
		goto out;
	/* Unknown supply state is not evidence that a requested cap is dispensable. */
	if (!soc_ret && !usb_ret &&
	    (type != POWER_SUPPLY_USB_TYPE_SDP || depletion < BOOT_SOCD_THRESHOLD)) {
		bcl_soc_clear_cpu_boot_limit(bcl);
	} else {
		mod_delayed_work(system_highpri_wq, &bcl->boot_work, RETRY_DELAY);
	}
out:
	mutex_unlock(&bcl->state_trans_lock);
}

static int battery_supply_callback(struct notifier_block *nb, unsigned long event, void *data)
{
	struct bcl_device *bcl = container_of(nb, struct bcl_device, psy_nb);
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED || (psy != bcl->batt_psy && psy != bcl->usb_psy))
		return NOTIFY_DONE;
	mutex_lock(&bcl->state_trans_lock);
	if (bcl->ready && !bcl->stopping) {
		schedule_work(&bcl->soc_eval_work);
		if (bcl->boot_limit_enabled)
			mod_delayed_work(system_highpri_wq, &bcl->boot_work, 0);
	}
	mutex_unlock(&bcl->state_trans_lock);
	return NOTIFY_OK;
}

static void bcl_soc_quiesce(void *data)
{
	struct bcl_device *bcl = data;

	mutex_lock(&bcl->state_trans_lock);
	bcl->stopping = true;
	bcl->ready = false;
	mutex_unlock(&bcl->state_trans_lock);
	if (bcl->notifier_registered) {
		power_supply_unreg_notifier(&bcl->psy_nb);
		bcl->notifier_registered = false;
	}
	cancel_delayed_work_sync(&bcl->boot_work);
	cancel_work_sync(&bcl->soc_eval_work);
	bcl_soc_clear_cpu_boot_limit(bcl);
}

static void bcl_put_supply(void *data) { power_supply_put(data); }
static struct power_supply *bcl_get_supply(struct device *dev, const char *name)
{
	struct power_supply *psy = power_supply_get_by_name(name);
	int ret;

	if (!psy) {
		atomic_set(&bcl_waiting_supplies, 1);
		return ERR_PTR(-EPROBE_DEFER);
	}
	ret = devm_add_action_or_reset(dev, bcl_put_supply, psy);
	return ret ? ERR_PTR(ret) : psy;
}

static const struct thermal_zone_device_ops bcl_ops = {
	.get_temp = bcl_read_soc,
	.set_trips = bcl_set_soc,
	.get_trend = bcl_soc_get_trend,
};

static int bcl_soc_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct bcl_device *bcl;
	int ret;

	if (boot_frequency <= 0)
		return -EINVAL;
	bcl = devm_kzalloc(dev, sizeof(*bcl), GFP_KERNEL);
	if (!bcl)
		return -ENOMEM;
	bcl->dev = dev;
	bcl->trip_temp = INT_MAX;
	mutex_init(&bcl->state_trans_lock);
	INIT_WORK(&bcl->soc_eval_work, bcl_evaluate_soc);
	INIT_DELAYED_WORK(&bcl->boot_work, bcl_boot_work);
	bcl->psy_nb.notifier_call = battery_supply_callback;
	bcl->limits = devm_kcalloc(dev, nr_cpu_ids, sizeof(*bcl->limits), GFP_KERNEL);
	if (!bcl->limits)
		return -ENOMEM;
	bcl->batt_psy = bcl_get_supply(dev, "battery");
	if (IS_ERR(bcl->batt_psy))
		return PTR_ERR(bcl->batt_psy);
	bcl->usb_psy = bcl_get_supply(dev, "usb");
	if (IS_ERR(bcl->usb_psy))
		return PTR_ERR(bcl->usb_psy);
	ret = devm_add_action_or_reset(dev, bcl_soc_clear_cpu_boot_limit, bcl);
	if (ret)
		return ret;
	if (boot_frequency != INT_MAX) {
		ret = bcl_soc_trigger_cpu_boot_limit(bcl);
		if (ret)
			return ret;
	}
	bcl->tz_dev = devm_thermal_of_zone_register(dev, 0, bcl, &bcl_ops);
	if (IS_ERR(bcl->tz_dev))
		return PTR_ERR(bcl->tz_dev);
	/* This action runs before thermal-zone and supply-reference teardown. */
	ret = devm_add_action_or_reset(dev, bcl_soc_quiesce, bcl);
	if (ret)
		return ret;
	ret = power_supply_reg_notifier(&bcl->psy_nb);
	if (ret)
		return ret;
	bcl->notifier_registered = true;
	platform_set_drvdata(pdev, bcl);
	mutex_lock(&bcl->state_trans_lock);
	bcl->ready = true;
	schedule_work(&bcl->soc_eval_work);
	if (bcl->boot_limit_enabled)
		mod_delayed_work(system_highpri_wq, &bcl->boot_work, 0);
	mutex_unlock(&bcl->state_trans_lock);
	return 0;
}

static void bcl_soc_remove(struct platform_device *pdev)
{
	struct bcl_device *bcl = platform_get_drvdata(pdev);

	if (bcl)
		bcl_soc_quiesce(bcl);
	platform_set_drvdata(pdev, NULL);
}

static const struct of_device_id bcl_match[] = {
	{ .compatible = "qcom,msm-bcl-soc" }, {},
};
MODULE_DEVICE_TABLE(of, bcl_match);
static struct platform_driver bcl_driver = {
	.probe = bcl_soc_probe, .remove = bcl_soc_remove,
	.driver = { .name = BCL_DRIVER_NAME, .of_match_table = bcl_match },
};
static void bcl_retry_suppliers(struct work_struct *work)
{
	int ret;

	/* Native driver_attach serializes probes and skips already bound devices. */
	if (!atomic_xchg(&bcl_waiting_supplies, 0))
		return;
	ret = driver_attach(&bcl_driver.driver);
	if (ret) {
		atomic_set(&bcl_waiting_supplies, 1);
		pr_err_ratelimited("BCL supply retry failed: %d\n", ret);
	}
}
static DECLARE_WORK(bcl_supplier_work, bcl_retry_suppliers);

static int bcl_supply_available(struct notifier_block *nb, unsigned long event,
				void *data)
{
	struct power_supply *psy = data;

	if (event != PSY_EVENT_PROP_CHANGED || !psy ||
	    (strcmp(psy->desc->name, "battery") && strcmp(psy->desc->name, "usb")))
		return NOTIFY_DONE;
	if (atomic_read(&bcl_waiting_supplies))
		schedule_work(&bcl_supplier_work);
	return NOTIFY_OK;
}
static struct notifier_block bcl_supplier_nb = {
	.notifier_call = bcl_supply_available,
};

static int bcl_soc_driver_register(struct platform_driver *driver)
{
	int ret;

	ret = platform_driver_register(driver);
	if (ret)
		return ret;
	ret = power_supply_reg_notifier(&bcl_supplier_nb);
	if (ret) {
		platform_driver_unregister(driver);
		atomic_set(&bcl_waiting_supplies, 0);
		return ret;
	}
	/* Close the supply-registration window between probe and notifier setup. */
	schedule_work(&bcl_supplier_work);
	return 0;
}

static void bcl_soc_driver_unregister(struct platform_driver *driver)
{
	power_supply_unreg_notifier(&bcl_supplier_nb);
	cancel_work_sync(&bcl_supplier_work);
	platform_driver_unregister(driver);
	atomic_set(&bcl_waiting_supplies, 0);
}
module_driver(bcl_driver, bcl_soc_driver_register, bcl_soc_driver_unregister);
MODULE_DESCRIPTION("Qualcomm battery depletion thermal sensor");
MODULE_LICENSE("GPL");
