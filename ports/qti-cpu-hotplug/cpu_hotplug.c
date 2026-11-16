// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 * Native 7.3 per-device lifetime, lock ordering and restore-on-teardown port.
 */
#include <linux/cpu.h>
#include <linux/cpuhotplug.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/workqueue.h>

#define CPU_HOTPLUG_LEVEL 1

struct cpu_hot_ctx;

struct cpu_hot_cdev {
	struct cpu_hot_ctx *ctx;
	int cpu_id;
	struct mutex state_lock;	/* callbacks, exec work, online veto */
	bool cpu_hot_state;		/* requested mitigation */
	bool offlined;			/* this driver took the CPU offline */
	struct thermal_cooling_device *cdev;	/* ctx->lock */
	struct device_node *np;		/* referenced */
	char cdev_name[THERMAL_NAME_LENGTH];
	struct work_struct reg_work;
	struct work_struct exec_work;
};

struct cpu_hot_ctx {
	struct mutex lock;		/* lifecycle; never taken from cdev callbacks */
	bool stopping;
	struct list_head node;		/* cpu_hot_list, cpu_hot_list_lock */
	int count;
	struct cpu_hot_cdev ent[] __counted_by(count);
};

/*
 * Single-instance state like the factory driver: failing a multi-instance
 * startup callback makes the native rollback warn (stale cpuhp last node).
 */
static DEFINE_MUTEX(cpu_hot_hp_lock);
static int cpu_hot_hp_users;
static enum cpuhp_state cpu_hot_hp_state;
static DEFINE_MUTEX(cpu_hot_list_lock);
static LIST_HEAD(cpu_hot_list);

static struct cpu_hot_cdev *cpu_hot_find(struct cpu_hot_ctx *ctx, unsigned int cpu)
{
	int i;

	for (i = 0; i < ctx->count; i++)
		if (ctx->ent[i].cpu_id == cpu)
			return &ctx->ent[i];
	return NULL;
}

static int cpu_hot_hp_online(unsigned int cpu)
{
	struct cpu_hot_ctx *ctx;
	struct cpu_hot_cdev *c;
	int ret = 0;

	mutex_lock(&cpu_hot_list_lock);
	list_for_each_entry(ctx, &cpu_hot_list, node) {
		c = cpu_hot_find(ctx, cpu);
		if (!c)
			continue;
		mutex_lock(&c->state_lock);
		if (c->cpu_hot_state) {
			/* Factory vetoed onlining a thermally isolated CPU. */
			pr_debug("Offline CPU:%d\n", cpu);
			ret = -EBUSY;
		}
		mutex_unlock(&c->state_lock);
		if (!ret)
			queue_work(system_highpri_wq, &c->reg_work);
	}
	mutex_unlock(&cpu_hot_list_lock);
	return ret;
}

static int cpu_hot_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	struct cpu_hot_cdev *c = cdev->devdata;

	if (state > CPU_HOTPLUG_LEVEL)
		return -EINVAL;
	mutex_lock(&c->state_lock);
	if (c->cpu_hot_state != !!state) {
		c->cpu_hot_state = !!state;
		queue_work(system_highpri_wq, &c->exec_work);
	}
	mutex_unlock(&c->state_lock);
	return 0;
}

static int cpu_hot_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	struct cpu_hot_cdev *c = cdev->devdata;

	mutex_lock(&c->state_lock);
	*state = c->cpu_hot_state ? CPU_HOTPLUG_LEVEL : 0;
	mutex_unlock(&c->state_lock);
	return 0;
}

static int cpu_hot_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = CPU_HOTPLUG_LEVEL;
	return 0;
}

static const struct thermal_cooling_device_ops cpu_hot_cooling_ops = {
	.get_max_state = cpu_hot_get_max_state,
	.get_cur_state = cpu_hot_get_cur_state,
	.set_cur_state = cpu_hot_set_cur_state,
};

/* Converges the CPU to the latest request; hotplug runs without state_lock. */
static void cpu_hot_execute_cdev(struct work_struct *work)
{
	struct cpu_hot_cdev *c = container_of(work, struct cpu_hot_cdev, exec_work);
	bool want;
	int ret;

	mutex_lock(&c->state_lock);
	want = c->cpu_hot_state;
	mutex_unlock(&c->state_lock);

	if (want && cpu_online(c->cpu_id)) {
		ret = remove_cpu(c->cpu_id);
		if (ret < 0) {
			pr_err("CPU:%d offline error:%d\n", c->cpu_id, ret);
			return;
		}
		mutex_lock(&c->state_lock);
		c->offlined = true;
		mutex_unlock(&c->state_lock);
	} else if (!want && c->offlined) {
		ret = add_cpu(c->cpu_id);
		if (ret) {
			pr_err("CPU:%d online error:%d\n", c->cpu_id, ret);
			return;
		}
		mutex_lock(&c->state_lock);
		c->offlined = false;
		mutex_unlock(&c->state_lock);
	}
	/* A newer request arrived while hotplug ran: converge again. */
	mutex_lock(&c->state_lock);
	if (c->cpu_hot_state != want)
		queue_work(system_highpri_wq, &c->exec_work);
	mutex_unlock(&c->state_lock);
}

static void cpu_hot_register_cdev(struct work_struct *work)
{
	struct cpu_hot_cdev *c = container_of(work, struct cpu_hot_cdev, reg_work);
	struct cpu_hot_ctx *ctx = c->ctx;
	struct thermal_cooling_device *cdev;

	mutex_lock(&ctx->lock);
	if (ctx->stopping || c->cdev)
		goto out;
	cdev = thermal_of_cooling_device_register(c->np, 0, c->cdev_name, c, &cpu_hot_cooling_ops);
	if (IS_ERR(cdev)) {
		pr_err("Cooling register failed for %s, ret:%ld\n", c->cdev_name, PTR_ERR(cdev));
		goto out;
	}
	c->cdev = cdev;
	pr_debug("Cooling device [%s] registered.\n", c->cdev_name);
out:
	mutex_unlock(&ctx->lock);
}

static int cpu_hot_hp_get(void)
{
	int ret = 0;

	mutex_lock(&cpu_hot_hp_lock);
	if (!cpu_hot_hp_users) {
		ret = cpuhp_setup_state_nocalls(CPUHP_AP_ONLINE_DYN, "cpu-hotplug/cdev:online",
						cpu_hot_hp_online, NULL);
		if (ret < 0)
			goto out;
		cpu_hot_hp_state = ret;
		ret = 0;
	}
	cpu_hot_hp_users++;
out:
	mutex_unlock(&cpu_hot_hp_lock);
	return ret;
}

static void cpu_hot_hp_put(void *unused)
{
	mutex_lock(&cpu_hot_hp_lock);
	if (!--cpu_hot_hp_users)
		cpuhp_remove_state_nocalls(cpu_hot_hp_state);
	mutex_unlock(&cpu_hot_hp_lock);
}

static void cpu_hot_put_nodes(void *data)
{
	struct cpu_hot_ctx *ctx = data;
	int i;

	for (i = 0; i < ctx->count; i++)
		of_node_put(ctx->ent[i].np);
}

static void cpu_hot_teardown(void *data)
{
	struct cpu_hot_ctx *ctx = data;
	struct cpu_hot_cdev *c;
	int i, ret;

	mutex_lock(&ctx->lock);
	ctx->stopping = true;
	mutex_unlock(&ctx->lock);
	/* After removal the online callback can no longer reach this context. */
	mutex_lock(&cpu_hot_list_lock);
	list_del(&ctx->node);
	mutex_unlock(&cpu_hot_list_lock);
	for (i = 0; i < ctx->count; i++)
		cancel_work_sync(&ctx->ent[i].reg_work);
	/* Unregister outside state_lock: native unregister drains callbacks. */
	mutex_lock(&ctx->lock);
	for (i = 0; i < ctx->count; i++) {
		c = &ctx->ent[i];
		if (c->cdev)
			thermal_cooling_device_unregister(c->cdev);
		c->cdev = NULL;
	}
	mutex_unlock(&ctx->lock);
	for (i = 0; i < ctx->count; i++) {
		c = &ctx->ent[i];
		cancel_work_sync(&c->exec_work);
		mutex_lock(&c->state_lock);
		c->cpu_hot_state = false;
		mutex_unlock(&c->state_lock);
		if (c->offlined) {
			ret = add_cpu(c->cpu_id);
			if (ret)
				pr_err("CPU:%d online error:%d\n", c->cpu_id, ret);
			else
				c->offlined = false;
		}
	}
}

static int cpu_hot_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *child, *cpu_np;
	struct cpu_hot_ctx *ctx;
	struct device *cpu_dev;
	const char *alias;
	int n = 0, cpu, id, ret, i;

	for_each_available_child_of_node(dev->of_node, child)
		n++;
	if (!n)
		return -ENODEV;
	ctx = devm_kzalloc(dev, struct_size(ctx, ent, n), GFP_KERNEL);
	if (!ctx)
		return -ENOMEM;
	mutex_init(&ctx->lock);
	ctx->count = n;
	n = 0;
	for_each_available_child_of_node(dev->of_node, child) {
		struct cpu_hot_cdev *c;

		cpu_np = of_parse_phandle(child, "qcom,cpu", 0);
		id = -1;
		for_each_possible_cpu(cpu) {
			cpu_dev = get_cpu_device(cpu);
			if (cpu_np && cpu_dev && cpu_dev->of_node == cpu_np) {
				id = cpu;
				break;
			}
		}
		of_node_put(cpu_np);
		for (i = 0; id >= 0 && i < n; i++)
			if (ctx->ent[i].cpu_id == id)
				id = -1;	/* duplicate CPU */
		if (id < 0 || n == ctx->count) {
			dev_err(dev, "Invalid CPU phandle in %pOF\n", child);
			continue;
		}
		c = &ctx->ent[n++];
		c->ctx = ctx;
		c->cpu_id = id;
		c->np = of_node_get(child);
		mutex_init(&c->state_lock);
		if (of_property_read_string(child, "qcom,cdev-alias", &alias))
			snprintf(c->cdev_name, THERMAL_NAME_LENGTH, "cpu-hotplug%d", id);
		else
			strscpy(c->cdev_name, alias, THERMAL_NAME_LENGTH);
		INIT_WORK(&c->reg_work, cpu_hot_register_cdev);
		INIT_WORK(&c->exec_work, cpu_hot_execute_cdev);
	}
	ctx->count = n;	/* trim unused slots */
	ret = devm_add_action_or_reset(dev, cpu_hot_put_nodes, ctx);
	if (ret)
		return ret;
	if (!ctx->count)
		return dev_err_probe(dev, -ENODEV, "no valid CPU hotplug cooling node\n");

	ret = cpu_hot_hp_get();
	if (ret)
		return ret;
	ret = devm_add_action_or_reset(dev, cpu_hot_hp_put, NULL);
	if (ret)
		return ret;
	mutex_lock(&cpu_hot_list_lock);
	list_add_tail(&ctx->node, &cpu_hot_list);
	mutex_unlock(&cpu_hot_list_lock);
	ret = devm_add_action_or_reset(dev, cpu_hot_teardown, ctx);
	if (ret)
		return ret;
	/* CPUs already online; later ones arrive through the online callback. */
	cpus_read_lock();
	for (i = 0; i < ctx->count; i++)
		if (cpu_online(ctx->ent[i].cpu_id))
			queue_work(system_highpri_wq, &ctx->ent[i].reg_work);
	cpus_read_unlock();
	platform_set_drvdata(pdev, ctx);
	return 0;
}

static const struct of_device_id cpu_hot_match[] = {
	{ .compatible = "qcom,cpu-hotplug", },
	{},
};
MODULE_DEVICE_TABLE(of, cpu_hot_match);

static struct platform_driver cpu_hot_driver = {
	.probe = cpu_hot_probe,
	.driver = {
		.name = KBUILD_MODNAME,
		.of_match_table = cpu_hot_match,
	},
};
module_platform_driver(cpu_hot_driver);
MODULE_DESCRIPTION("CPU Hotplug cooling device driver");
MODULE_LICENSE("GPL");
