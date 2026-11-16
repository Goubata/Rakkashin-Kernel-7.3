// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Native 7.3 port: per-device lifetime, lock ordering, referenced child nodes and
 * resume-on-teardown. Pause/resume semantics and the exported notifier/mask API
 * are unchanged (consumers: sched_walt PAUSE_THERMAL, vendor HAL by cdev type).
 */

#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/cpumask.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/sched/walt.h>
#include <linux/thermal_pause.h>
#include <linux/slab.h>
#include <linux/thermal.h>

enum thermal_pause_levels {
	THERMAL_NO_CPU_PAUSE,
	THERMAL_GROUP_CPU_PAUSE,

	/* define new pause levels above this line */
	MAX_THERMAL_PAUSE_LEVEL
};

#define THERMAL_PAUSE_RETRY_COUNT 5

struct thermal_pause_ctx;

struct thermal_pause_cdev {
	struct thermal_pause_ctx	*ctx;
	cpumask_t			cpu_mask;
	enum thermal_pause_levels	thermal_pause_level;	/* cpus_pause_lock */
	enum thermal_pause_levels	thermal_pause_req;	/* cpus_pause_lock */
	struct thermal_cooling_device	*cdev;			/* ctx->lock */
	struct device_node		*np;			/* referenced */
	char				cdev_name[THERMAL_NAME_LENGTH];
	struct work_struct		reg_work;
	struct work_struct		pause_update_work;
};

struct thermal_pause_ctx {
	struct mutex		lock;		/* cdev lifecycle; never taken by callbacks */
	bool			stopping;
	struct list_head	node;		/* thermal_pause_ctx_list */
	int			count;
	struct thermal_pause_cdev ent[] __counted_by(count);
};

/* Pause state shared by all instances, as in the factory driver. */
static DEFINE_MUTEX(cpus_pause_lock);
static struct cpumask cpus_in_max_cooling_level;
static BLOCKING_NOTIFIER_HEAD(thermal_pause_notifier);

/* Single-instance online callback walks this list (veto-free, registration only). */
static DEFINE_MUTEX(thermal_pause_list_lock);
static LIST_HEAD(thermal_pause_ctx_list);
static DEFINE_MUTEX(thermal_pause_hp_lock);
static int thermal_pause_hp_users;
static enum cpuhp_state thermal_pause_hp_state;

void thermal_pause_notifier_register(struct notifier_block *n)
{
	blocking_notifier_chain_register(&thermal_pause_notifier, n);
}
EXPORT_SYMBOL_GPL(thermal_pause_notifier_register);

void thermal_pause_notifier_unregister(struct notifier_block *n)
{
	blocking_notifier_chain_unregister(&thermal_pause_notifier, n);
}
EXPORT_SYMBOL_GPL(thermal_pause_notifier_unregister);

const struct cpumask *thermal_paused_cpumask(void)
{
	return &cpus_in_max_cooling_level;
}
EXPORT_SYMBOL_GPL(thermal_paused_cpumask);

static int thermal_pause_hp_online(unsigned int online_cpu)
{
	struct thermal_pause_ctx *ctx;
	int i;

	mutex_lock(&thermal_pause_list_lock);
	list_for_each_entry(ctx, &thermal_pause_ctx_list, node)
		for (i = 0; i < ctx->count; i++)
			if (cpumask_test_cpu(online_cpu, &ctx->ent[i].cpu_mask))
				queue_work(system_highpri_wq, &ctx->ent[i].reg_work);
	mutex_unlock(&thermal_pause_list_lock);
	return 0;
}

/* cpus_pause_lock held on entry and exit; dropped around WALT. */
static int thermal_pause_work(struct thermal_pause_cdev *c)
{
	cpumask_t cpus_to_pause, cpus_to_notify;
	int cpu, ret;

	cpumask_copy(&cpus_to_pause, &c->cpu_mask);
	pr_debug("Pause:%*pbl\n", cpumask_pr_args(&c->cpu_mask));

	mutex_unlock(&cpus_pause_lock);
	ret = walt_pause_cpus(&cpus_to_pause, PAUSE_THERMAL);
	mutex_lock(&cpus_pause_lock);

	if (ret == 0) {
		/* remove CPUs that have already been notified */
		cpumask_andnot(&cpus_to_notify, &c->cpu_mask, &cpus_in_max_cooling_level);
		for_each_cpu(cpu, &cpus_to_notify)
			blocking_notifier_call_chain(&thermal_pause_notifier, 1,
						     (void *)(long)cpu);
		cpumask_or(&cpus_in_max_cooling_level, &cpus_in_max_cooling_level,
			   &c->cpu_mask);
	} else {
		pr_err("Error pausing CPU:%*pbl. err:%d\n",
		       cpumask_pr_args(&c->cpu_mask), ret);
	}
	return ret;
}

/* Union of the masks of every other paused cdev of every instance. */
static void thermal_pause_others(struct thermal_pause_cdev *self, cpumask_t *out)
{
	struct thermal_pause_ctx *ctx;
	int i;

	cpumask_clear(out);
	mutex_lock(&thermal_pause_list_lock);
	list_for_each_entry(ctx, &thermal_pause_ctx_list, node)
		for (i = 0; i < ctx->count; i++) {
			struct thermal_pause_cdev *c = &ctx->ent[i];

			if (c != self && c->thermal_pause_level)
				cpumask_or(out, out, &c->cpu_mask);
		}
	mutex_unlock(&thermal_pause_list_lock);
}

static int thermal_resume_work(struct thermal_pause_cdev *c)
{
	cpumask_t cpus_to_unpause, new_paused_cpus, cpus_to_notify;
	int cpu, ret;

	/*
	 * WALT keeps one vote bit per client, not a count: resuming a CPU that another
	 * thermal_pause cdev still holds would release that cdev's pause too (factory
	 * bug with overlapping per-CPU and group cdevs). Resume only unshared CPUs.
	 */
	thermal_pause_others(c, &new_paused_cpus);
	cpumask_andnot(&cpus_to_unpause, &c->cpu_mask, &new_paused_cpus);
	pr_debug("Unpause:%*pbl\n", cpumask_pr_args(&cpus_to_unpause));

	ret = 0;
	if (!cpumask_empty(&cpus_to_unpause)) {
		mutex_unlock(&cpus_pause_lock);
		ret = walt_resume_cpus(&cpus_to_unpause, PAUSE_THERMAL);
		mutex_lock(&cpus_pause_lock);
	}

	if (ret == 0) {
		/* others may have changed while unlocked */
		thermal_pause_others(c, &new_paused_cpus);
		/* Notify for each CPU that leaves the thermal-paused set. */
		cpumask_andnot(&cpus_to_notify, &cpus_in_max_cooling_level, &new_paused_cpus);
		for_each_cpu(cpu, &cpus_to_notify)
			blocking_notifier_call_chain(&thermal_pause_notifier, 0,
						     (void *)(long)cpu);
		cpumask_copy(&cpus_in_max_cooling_level, &new_paused_cpus);
	} else {
		/* Ref-count for cpus controlled by thermal still set */
		pr_err("Error resuming CPU:%*pbl. err:%d\n",
		       cpumask_pr_args(&c->cpu_mask), ret);
	}
	return ret;
}

/* cpus_pause_lock held. Converge to the latest request, bounded retries on failure. */
static void thermal_pause_converge(struct thermal_pause_cdev *c)
{
	int retcnt = THERMAL_PAUSE_RETRY_COUNT;
	int ret = 0;

retry:
	if (c->thermal_pause_req != c->thermal_pause_level) {
		if (c->thermal_pause_req == THERMAL_NO_CPU_PAUSE) {
			ret = thermal_resume_work(c);
			if (ret >= 0)
				c->thermal_pause_level = THERMAL_NO_CPU_PAUSE;
		} else {
			ret = thermal_pause_work(c);
			if (ret >= 0)
				c->thermal_pause_level = THERMAL_GROUP_CPU_PAUSE;
		}
		if (ret < 0 && retcnt > 0) {
			retcnt--;
			goto retry;
		}
	}
	if (ret >= 0 && c->thermal_pause_req != c->thermal_pause_level) {
		pr_debug("Pause: requested state changed while workfn running\n");
		retcnt = THERMAL_PAUSE_RETRY_COUNT;
		goto retry;
	}
}

static void thermal_pause_update_work(struct work_struct *work)
{
	struct thermal_pause_cdev *c = container_of(work, struct thermal_pause_cdev,
						    pause_update_work);

	mutex_lock(&cpus_pause_lock);
	thermal_pause_converge(c);
	mutex_unlock(&cpus_pause_lock);
}

static int thermal_pause_set_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long level)
{
	struct thermal_pause_cdev *c = cdev->devdata;

	if (level >= MAX_THERMAL_PAUSE_LEVEL)
		return -EINVAL;

	mutex_lock(&cpus_pause_lock);
	c->thermal_pause_req = level ? THERMAL_GROUP_CPU_PAUSE : THERMAL_NO_CPU_PAUSE;
	queue_work(system_highpri_wq, &c->pause_update_work);
	mutex_unlock(&cpus_pause_lock);
	return 0;
}

static int thermal_pause_get_cur_state(struct thermal_cooling_device *cdev,
				       unsigned long *level)
{
	struct thermal_pause_cdev *c = cdev->devdata;

	*level = READ_ONCE(c->thermal_pause_level);
	return 0;
}

static int thermal_pause_get_max_state(struct thermal_cooling_device *cdev,
				       unsigned long *level)
{
	*level = MAX_THERMAL_PAUSE_LEVEL - 1;
	return 0;
}

static const struct thermal_cooling_device_ops thermal_pause_cooling_ops = {
	.get_max_state = thermal_pause_get_max_state,
	.get_cur_state = thermal_pause_get_cur_state,
	.set_cur_state = thermal_pause_set_cur_state,
};

static void thermal_pause_register_cdev(struct work_struct *work)
{
	struct thermal_pause_cdev *c = container_of(work, struct thermal_pause_cdev, reg_work);
	struct thermal_pause_ctx *ctx = c->ctx;
	struct thermal_cooling_device *cdev;
	cpumask_t cpus_online;

	mutex_lock(&ctx->lock);
	if (ctx->stopping || c->cdev)
		goto out;
	/* Factory: register only once every CPU of the group is online. */
	cpumask_and(&cpus_online, &c->cpu_mask, cpu_online_mask);
	if (!cpumask_equal(&c->cpu_mask, &cpus_online))
		goto out;
	cdev = thermal_of_cooling_device_register(c->np, 0, c->cdev_name, c,
						  &thermal_pause_cooling_ops);
	if (IS_ERR(cdev)) {
		pr_err("Cooling register failed for %s, ret:%ld\n", c->cdev_name, PTR_ERR(cdev));
		goto out;
	}
	c->cdev = cdev;
	pr_debug("Cooling device [%s] registered.\n", c->cdev_name);
out:
	mutex_unlock(&ctx->lock);
}

static int thermal_pause_hp_get(void)
{
	int ret = 0;

	mutex_lock(&thermal_pause_hp_lock);
	if (!thermal_pause_hp_users) {
		ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "thermal-pause/cdev:online",
						thermal_pause_hp_online, NULL);
		if (ret < 0)
			goto out;
		thermal_pause_hp_state = ret;
		ret = 0;
	}
	thermal_pause_hp_users++;
out:
	mutex_unlock(&thermal_pause_hp_lock);
	return ret;
}

static void thermal_pause_hp_put(void *unused)
{
	mutex_lock(&thermal_pause_hp_lock);
	if (!--thermal_pause_hp_users)
		cpuhp_remove_state_nocalls(thermal_pause_hp_state);
	mutex_unlock(&thermal_pause_hp_lock);
}

static void thermal_pause_put_nodes(void *data)
{
	struct thermal_pause_ctx *ctx = data;
	int i;

	for (i = 0; i < ctx->count; i++)
		of_node_put(ctx->ent[i].np);
}

static void thermal_pause_teardown(void *data)
{
	struct thermal_pause_ctx *ctx = data;
	struct thermal_pause_cdev *c;
	int i;

	mutex_lock(&ctx->lock);
	ctx->stopping = true;
	mutex_unlock(&ctx->lock);
	for (i = 0; i < ctx->count; i++)
		cancel_work_sync(&ctx->ent[i].reg_work);
	/* Unregister without holding locks the callbacks take (drains them). */
	mutex_lock(&ctx->lock);
	for (i = 0; i < ctx->count; i++) {
		c = &ctx->ent[i];
		if (c->cdev)
			thermal_cooling_device_unregister(c->cdev);
		c->cdev = NULL;
	}
	mutex_unlock(&ctx->lock);
	/* No new requests can arrive: finish pending work, then resume what we paused. */
	for (i = 0; i < ctx->count; i++)
		cancel_work_sync(&ctx->ent[i].pause_update_work);
	mutex_lock(&cpus_pause_lock);
	for (i = 0; i < ctx->count; i++) {
		c = &ctx->ent[i];
		c->thermal_pause_req = THERMAL_NO_CPU_PAUSE;
		thermal_pause_converge(c);
	}
	mutex_unlock(&cpus_pause_lock);
	/* Removing from the list last keeps resume's union of other cdevs exact. */
	mutex_lock(&thermal_pause_list_lock);
	list_del(&ctx->node);
	mutex_unlock(&thermal_pause_list_lock);
}

static int thermal_pause_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct thermal_pause_ctx *ctx;
	struct device_node *child;
	struct of_phandle_iterator it;
	struct device *cpu_dev;
	const char *alias;
	int n = 0, cpu, ret, err, i;

	for_each_available_child_of_node(dev->of_node, child)
		n++;
	if (!n)
		return -ENODEV;
	ctx = devm_kzalloc(dev, struct_size(ctx, ent, n), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mutex_init(&ctx->lock);
	INIT_LIST_HEAD(&ctx->node);
	ctx->count = n;
	n = 0;
	for_each_available_child_of_node(dev->of_node, child) {
		struct thermal_pause_cdev *c;
		cpumask_t mask;
		unsigned long bits = 0;

		cpumask_clear(&mask);
		of_for_each_phandle(&it, err, child, "qcom,cpus", NULL, 0) {
			for_each_possible_cpu(cpu) {
				cpu_dev = get_cpu_device(cpu);
				if (cpu_dev && cpu_dev->of_node == it.node) {
					cpumask_set_cpu(cpu, &mask);
					bits |= BIT(cpu);
					break;
				}
			}
		}
		if (cpumask_empty(&mask) || n == ctx->count)
			continue;
		c = &ctx->ent[n++];
		c->ctx = ctx;
		c->np = of_node_get(child);
		cpumask_copy(&c->cpu_mask, &mask);
		if (of_property_read_string(child, "qcom,cdev-alias", &alias))
			snprintf(c->cdev_name, THERMAL_NAME_LENGTH, "thermal-pause-%lX", bits);
		else
			strscpy(c->cdev_name, alias, THERMAL_NAME_LENGTH);
		INIT_WORK(&c->reg_work, thermal_pause_register_cdev);
		INIT_WORK(&c->pause_update_work, thermal_pause_update_work);
	}
	ctx->count = n;	/* trim unused slots */
	ret = devm_add_action_or_reset(dev, thermal_pause_put_nodes, ctx);
	if (ret)
		return ret;
	if (!ctx->count)
		return dev_err_probe(dev, -ENODEV, "no pause group maps to a possible CPU\n");

	ret = thermal_pause_hp_get();
	if (ret)
		return ret;
	ret = devm_add_action_or_reset(dev, thermal_pause_hp_put, NULL);
	if (ret)
		return ret;
	mutex_lock(&thermal_pause_list_lock);
	list_add_tail(&ctx->node, &thermal_pause_ctx_list);
	mutex_unlock(&thermal_pause_list_lock);
	ret = devm_add_action_or_reset(dev, thermal_pause_teardown, ctx);
	if (ret)
		return ret;
	/* Groups whose CPUs are online now; others register from the online callback. */
	for (i = 0; i < ctx->count; i++)
		queue_work(system_highpri_wq, &ctx->ent[i].reg_work);
	platform_set_drvdata(pdev, ctx);
	return 0;
}

static const struct of_device_id thermal_pause_match[] = {
	{ .compatible = "qcom,thermal-pause", },
	{},
};
MODULE_DEVICE_TABLE(of, thermal_pause_match);

static struct platform_driver thermal_pause_driver = {
	.probe = thermal_pause_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = thermal_pause_match,
	},
};

module_platform_driver(thermal_pause_driver);
MODULE_LICENSE("GPL v2");
