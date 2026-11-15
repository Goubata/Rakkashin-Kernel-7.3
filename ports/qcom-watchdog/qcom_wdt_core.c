// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2012-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */
#include <linux/irqdomain.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/percpu.h>
#include <linux/of.h>
#include <linux/cpu_pm.h>
#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/reboot.h>
#include <linux/panic_notifier.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <soc/qcom/minidump.h>
#include <soc/qcom/watchdog.h>
#include <linux/soc/qcom/qcom_sdei.h>
#include <linux/cpumask.h>
#include <linux/cpu_pm.h>
#include <uapi/linux/sched/types.h>
#include <linux/sched/clock.h>
#include <linux/irq.h>
#include <linux/sort.h>
#include <linux/kernel_stat.h>
#include <linux/kallsyms.h>
#include <linux/kdebug.h>
#include <asm/hardirq.h>
#include <linux/suspend.h>
#include <linux/notifier.h>

#define MASK_SIZE        32

static struct msm_watchdog_data *wdog_data;
static DEFINE_MUTEX(wdog_registration_lock);
static const struct attribute_group qcom_wdt_attr_group;

/* Retain the exact descriptor only after successful registration. */
static int qcom_wdt_add_dump(struct msm_watchdog_data *wdog_dd)
{
	struct md_region *entry = &wdog_dd->dump_entry;
	int ret;

	if (wdog_dd->dump_registered)
		return -EBUSY;
	memset(entry, 0, sizeof(*entry));
	strscpy(entry->name, "KWDOGDATA", sizeof(entry->name));
	entry->virt_addr = (uintptr_t)wdog_dd;
	entry->phys_addr = virt_to_phys(wdog_dd);
	entry->size = sizeof(*wdog_dd);
	ret = msm_minidump_add_region(entry);
	if (ret >= 0)
		wdog_dd->dump_registered = true;
	else
		dev_err(wdog_dd->dev, "Failed to add Wdt data in Minidump: %d\n", ret);
	return ret;
}

static int qcom_wdt_remove_dump(struct msm_watchdog_data *wdog_dd)
{
	int ret;

	if (!wdog_dd->dump_registered)
		return 0;
	ret = msm_minidump_remove_region(&wdog_dd->dump_entry);
	if (!ret || ret == -ENOENT) {
		wdog_dd->dump_registered = false;
		return 0;
	}
	dev_err(wdog_dd->dev, "Minidump retains Wdt data: %d\n", ret);
	return ret;
}

static void qcom_wdt_dump_cpu_alive_mask(struct msm_watchdog_data *wdog_dd)
{
	static char alive_mask_buf[MASK_SIZE];

	scnprintf(alive_mask_buf, MASK_SIZE, "%*pb1", cpumask_pr_args(
				&wdog_dd->alive_mask));
	dev_info(wdog_dd->dev, "cpu alive mask from last pet %s\n",
				alive_mask_buf);
}

#ifdef CONFIG_QCOM_IRQ_STAT
static void record_irq_count(void)
{
	int irq, ipi_nr;
	unsigned int count;
	struct irq_desc *desc;

	ipi_nr = nr_ipi_get();

	rcu_read_lock();
	for_each_irq_nr(irq) {
		if (irq <= ipi_nr)
			continue;

		desc = irq_to_desc(irq);
		if (!desc)
			continue;

		count = kstat_irqs_usr(irq);
		desc->android_vendor_data1 = count;
	}
	rcu_read_unlock();
}

struct irq_info {
	int irq;
	unsigned int count;
	struct irq_desc *desc;
};

static void compute_irq_count(void)
{
	int i, irq, ipi_nr;
	unsigned int count, diff;
	struct irq_desc *desc;
	unsigned long time_diff;
	struct irq_info irq_info_list[NR_TOP_HITTERS];
	unsigned int list_min_cnt = 0, list_min_pos = 0;

	ipi_nr = nr_ipi_get();
	time_diff = (sched_clock() - wdog_data->last_pet) / 1000000;

	rcu_read_lock();
	for_each_irq_nr(irq) {
		if (irq <= ipi_nr)
			continue;

		desc = irq_to_desc(irq);
		if (!desc)
			continue;

		count = kstat_irqs_usr(irq);
		diff = count - desc->android_vendor_data1;

		if (diff > list_min_cnt) {
			irq_info_list[list_min_pos].count = diff;
			irq_info_list[list_min_pos].irq = irq;
			irq_info_list[list_min_pos].desc = desc;

			list_min_pos = 0;
			list_min_cnt = irq_info_list[0].count;
			for (i = 1; i < NR_TOP_HITTERS; i++) {
				if (irq_info_list[i].count < list_min_cnt) {
					list_min_pos = i;
					list_min_cnt = irq_info_list[i].count;
				}
			}
		}
	}
	rcu_read_unlock();

	pr_emerg("Top irqs in last %lu ms:\n", time_diff);
	for (i = 0; i < NR_TOP_HITTERS; i++) {
		count = irq_info_list[i].count;
		if (count != 0) {
			irq = irq_info_list[i].irq;
			desc = irq_info_list[i].desc;
			pr_emerg("IRQ %d [%s:%s] - %d times\n", irq,
				(desc->irq_data.chip) ?
				desc->irq_data.chip->name : "UNKNOWN",
				(desc->action) ?
				desc->action->name : "NONAME",
				count);
		}
	}
}
#else
static void record_irq_count(void) { }
static void compute_irq_count(void) { }
#endif

static int qcom_wdt_hibernation_notifier(struct notifier_block *nb,
				unsigned long event, void *dummy)
{
	struct msm_watchdog_data *data = READ_ONCE(wdog_data);

	if (!data || READ_ONCE(data->stopping))
		return NOTIFY_DONE;
	if ((event == PM_HIBERNATION_PREPARE) || ((event == PM_SUSPEND_PREPARE)
			&& (pm_suspend_target_state == PM_SUSPEND_MEM)))
		data->hibernate = true;
	else if ((event == PM_POST_HIBERNATION) || ((event == PM_POST_SUSPEND)
			&& (pm_suspend_target_state == PM_SUSPEND_MEM)))
		data->hibernate = false;
	return NOTIFY_OK;
}

static struct notifier_block qcom_wdt_notif_block = {
	.notifier_call = qcom_wdt_hibernation_notifier,
};

#ifdef CONFIG_PM_SLEEP
/**
 *  qcom_wdt_pet_suspend() - Suspends qcom watchdog functionality.
 *
 *  @dev: qcom watchdog device structure
 *
 *  All watchdogs should have the ability to be suspended, this
 *  will cause the watchdog counter to reset and HW counter will
 *  freeze when deepest low power mode is entered.
 *
 */
int qcom_wdt_pet_suspend(struct device *dev)
{
	struct msm_watchdog_data *wdog_data =
			(struct msm_watchdog_data *)dev_get_drvdata(dev);

	if (!wdog_data || READ_ONCE(wdog_data->stopping) ||
	    READ_ONCE(wdog_data->user_disabled))
		return 0;

	mutex_lock(&wdog_data->disable_lock);
	if (wdog_data->stopping || wdog_data->user_disabled) {
		mutex_unlock(&wdog_data->disable_lock);
		return 0;
	}

	if (wdog_data->user_pet_enabled)
		timer_delete_sync(&wdog_data->user_pet_timer);

	spin_lock(&wdog_data->freeze_lock);
	wdog_data->freeze_in_progress = true;
	spin_unlock(&wdog_data->freeze_lock);
	wdog_data->ops->reset_wdt(wdog_data);
	timer_delete_sync(&wdog_data->pet_timer);
	if (wdog_data->wakeup_irq_enable) {
		if (wdog_data->hibernate || (pm_suspend_target_state == PM_SUSPEND_MEM)) {
			wdog_data->ops->disable_wdt(wdog_data);
			wdog_data->enabled = false;
		}
		wdog_data->last_pet = sched_clock();
		mutex_unlock(&wdog_data->disable_lock);
		return 0;
	}

	wdog_data->ops->disable_wdt(wdog_data);
	wdog_data->enabled = false;
	wdog_data->last_pet = sched_clock();
	mutex_unlock(&wdog_data->disable_lock);
	return 0;
}
EXPORT_SYMBOL(qcom_wdt_pet_suspend);

/**
 *  qcom_wdt_pet_resume() - Resumes qcom watchdog after a suspend.
 *
 *  @dev: qcom watchdog device structure
 *
 *  All watchdogs should have the ability to be resumed after a suspend.
 *  This will cause the watchdog counter to be reset and resumed.
 *
 */
int qcom_wdt_pet_resume(struct device *dev)
{
	uint32_t val;
	struct msm_watchdog_data *wdog_data =
			(struct msm_watchdog_data *)dev_get_drvdata(dev);
	unsigned long delay_time = 0;

	if (!wdog_data || READ_ONCE(wdog_data->stopping) ||
	    READ_ONCE(wdog_data->user_disabled))
		return 0;

	mutex_lock(&wdog_data->disable_lock);
	if (wdog_data->stopping || wdog_data->user_disabled) {
		mutex_unlock(&wdog_data->disable_lock);
		return 0;
	}

	val = BIT(EN);
	if (wdog_data->user_pet_enabled) {
		delay_time = msecs_to_jiffies(wdog_data->bark_time + 3 * 1000);
		wdog_data->user_pet_timer.expires = jiffies + delay_time;
		add_timer(&wdog_data->user_pet_timer);
	}

	delay_time = msecs_to_jiffies(wdog_data->pet_time);
	spin_lock(&wdog_data->freeze_lock);
	wdog_data->pet_timer.expires = jiffies + delay_time;
	add_timer(&wdog_data->pet_timer);
	wdog_data->freeze_in_progress = false;
	spin_unlock(&wdog_data->freeze_lock);
	if (wdog_data->wakeup_irq_enable) {
		if (wdog_data->hibernate || (pm_suspend_target_state == PM_SUSPEND_MEM)) {
			wdog_data->ops->set_bark_time(wdog_data->bark_time, wdog_data);
			wdog_data->ops->set_bite_time(wdog_data->bark_time + 3 * 1000, wdog_data);
			val |= BIT(UNMASKED_INT_EN);
			wdog_data->ops->enable_wdt(val, wdog_data);
			wdog_data->enabled = true;
		}
		wdog_data->ops->reset_wdt(wdog_data);
		wdog_data->last_pet = sched_clock();
		mutex_unlock(&wdog_data->disable_lock);
		return 0;
	}

	wdog_data->ops->enable_wdt(val, wdog_data);
	wdog_data->ops->reset_wdt(wdog_data);
	wdog_data->enabled = true;
	wdog_data->last_pet = sched_clock();
	mutex_unlock(&wdog_data->disable_lock);
	return 0;
}
EXPORT_SYMBOL(qcom_wdt_pet_resume);
#endif

static void qcom_wdt_reset_on_oops(struct msm_watchdog_data *wdog_dd,
			int timeout)
{
	wdog_dd->ops->reset_wdt(wdog_dd);
	wdog_dd->ops->set_bark_time((timeout + 10) * 1000,
					wdog_dd);
	wdog_dd->ops->set_bite_time((timeout + 10) * 1000,
					wdog_dd);
}

static int qcom_wdt_panic_handler(struct notifier_block *this,
			      unsigned long event, void *ptr)
{
	struct msm_watchdog_data *wdog_dd = container_of(this,
				struct msm_watchdog_data, panic_blk);

	wdog_dd->in_panic = true;
	if (WDOG_BITE_EARLY_PANIC) {
		pr_info("Triggering early bite\n");
		qcom_wdt_trigger_bite();
	}
	if (panic_timeout == 0)
		wdog_dd->ops->disable_wdt(wdog_dd);
	else
		qcom_wdt_reset_on_oops(wdog_dd, panic_timeout);

	return NOTIFY_DONE;
}

#ifdef CONFIG_QCOM_MINIDUMP_PANIC_DUMP
static int qcom_wdt_die_handler(struct notifier_block *this,
				unsigned long val, void *data)
{
	struct msm_watchdog_data *wdog_dd = container_of(this,
				struct msm_watchdog_data, die_blk);

	qcom_wdt_reset_on_oops(wdog_dd, 5);
	return NOTIFY_DONE;
}

static int qcom_wdt_register_die_notifier(struct msm_watchdog_data *wdog_dd)
{
	int ret;

	wdog_dd->die_blk.notifier_call = qcom_wdt_die_handler;
	wdog_dd->die_blk.priority = INT_MAX - 1;
	ret = register_die_notifier(&wdog_dd->die_blk);

	if (!ret)
		wdog_dd->die_registered = true;
	return ret;
}

static void qcom_wdt_unregister_die_notifier(struct msm_watchdog_data *wdog_dd)
{
	unregister_die_notifier(&wdog_dd->die_blk);
}
#else
static int qcom_wdt_register_die_notifier(struct msm_watchdog_data *wdog_dd) { return 0; }
static void qcom_wdt_unregister_die_notifier(struct msm_watchdog_data *wdog_dd) { }
#endif

/* disable_lock serializes hardware state; resource flags describe ownership
 * independently of enabled (which also becomes false during suspend). */
static void qcom_wdt_disable(struct msm_watchdog_data *wdog_dd)
{
	WRITE_ONCE(wdog_dd->user_disabled, true);
	WRITE_ONCE(wdog_dd->enabled, false);
	spin_lock(&wdog_dd->freeze_lock);
	wdog_dd->freeze_in_progress = true;
	spin_unlock(&wdog_dd->freeze_lock);
	wdog_dd->ops->disable_wdt(wdog_dd);

	if (wdog_dd->cpu_pm_registered) {
		cpu_pm_unregister_notifier(&wdog_dd->wdog_cpu_pm_nb);
		wdog_dd->cpu_pm_registered = false;
	}
	if (wdog_dd->pm_registered) {
		unregister_pm_notifier(&qcom_wdt_notif_block);
		wdog_dd->pm_registered = false;
	}
	if (wdog_dd->irq_registered) {
		if (wdog_dd->irq_ppi) {
			if (wdog_dd->irq_enabled)
				disable_percpu_irq(wdog_dd->bark_irq);
			free_percpu_irq(wdog_dd->bark_irq,
					(void __percpu *)wdog_dd->wdog_cpu_dd);
		} else {
			devm_free_irq(wdog_dd->dev, wdog_dd->bark_irq, wdog_dd);
		}
		wdog_dd->irq_registered = false;
		wdog_dd->irq_enabled = false;
	}
	if (wdog_dd->wdog_cpu_dd) {
		free_percpu(wdog_dd->wdog_cpu_dd);
		wdog_dd->wdog_cpu_dd = NULL;
	}
	if (wdog_dd->panic_registered) {
		atomic_notifier_chain_unregister(&panic_notifier_list, &wdog_dd->panic_blk);
		wdog_dd->panic_registered = false;
	}
	if (wdog_dd->die_registered) {
		qcom_wdt_unregister_die_notifier(wdog_dd);
		wdog_dd->die_registered = false;
	}
	if (wdog_dd->restart_registered) {
		unregister_restart_handler(&wdog_dd->restart_blk);
		wdog_dd->restart_registered = false;
	}
	if (wdog_dd->timers_initialized) {
		timer_shutdown_sync(&wdog_dd->pet_timer);
		timer_shutdown_sync(&wdog_dd->user_pet_timer);
		wdog_dd->timers_initialized = false;
	}
}

static int restart_wdog_handler(struct notifier_block *this,
			       unsigned long event, void *ptr)
{
	struct msm_watchdog_data *wdog_dd = container_of(this,
				struct msm_watchdog_data, restart_blk);
	if (WDOG_BITE_ON_PANIC && wdog_dd->in_panic) {
		/*
		 * Trigger a watchdog bite here and if this fails,
		 * device will take the usual restart path.
		 */
		pr_info("Triggering late bite\n");
		qcom_wdt_trigger_bite();
	}
	return NOTIFY_DONE;
}

static ssize_t qcom_wdt_disable_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);
	int ret;
	int disable_val;

	mutex_lock(&wdog_dd->disable_lock);
	disable_val  = wdog_dd->enabled ? 0 : 1;
	ret = scnprintf(buf, PAGE_SIZE, "%d\n", disable_val);
	mutex_unlock(&wdog_dd->disable_lock);
	return ret;
}

static ssize_t qcom_wdt_disable_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);
	u8 disable;
	int ret;

	ret = kstrtou8(buf, 10, &disable);
	if (ret) {
		dev_err(wdog_dd->dev, "invalid user input\n");
		return ret;
	}
	if (!wdog_dd)
		return -ENODEV;
	if (disable == 1) {
		mutex_lock(&wdog_dd->disable_lock);
		if (!smp_load_acquire(&wdog_dd->ready)) {
			mutex_unlock(&wdog_dd->disable_lock);
			return -ENODEV;
		}
		if (wdog_dd->user_disabled) {
			dev_err(wdog_dd->dev, "MSM Apps Watchdog already disabled\n");
			mutex_unlock(&wdog_dd->disable_lock);
			return count;
		}
		ret = qcom_scm_sec_wdog_deactivate();
		if (ret) {
			dev_err(wdog_dd->dev,
				"Failed to deactivate secure wdog, ret = %d\n",
				 ret);
			mutex_unlock(&wdog_dd->disable_lock);
			return ret;
		}
		qcom_wdt_disable(wdog_dd);
		mutex_unlock(&wdog_dd->disable_lock);
	} else {
		pr_err("invalid operation, only disable = 1 supported\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(disable, 0600, qcom_wdt_disable_get, qcom_wdt_disable_set);

/*
 * Userspace Watchdog Support:
 * Write 1 to the "user_pet_enabled" file to enable hw support for a
 * userspace watchdog.
 * Userspace is required to pet the watchdog by continuing to write 1
 * to this file in the expected interval.
 * Userspace may disable this requirement by writing 0 to this same
 * file.
 */
static void __qcom_wdt_user_pet(struct msm_watchdog_data *wdog_dd)
{
	WRITE_ONCE(wdog_dd->user_pet_complete, true);
	wake_up(&wdog_dd->pet_complete);
}

static ssize_t qcom_wdt_user_pet_enabled_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n",
			wdog_dd->user_pet_enabled);
	return ret;
}

static ssize_t qcom_wdt_user_pet_enabled_set(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);
	bool enable, already_enabled;
	unsigned long delay_time;
	int ret = kstrtobool(buf, &enable);

	if (ret)
		return ret;
	if (!wdog_dd)
		return -ENODEV;
	mutex_lock(&wdog_dd->disable_lock);
	if (!smp_load_acquire(&wdog_dd->ready)) {
		ret = -ENODEV;
		goto out;
	}
	if (wdog_dd->user_disabled) {
		ret = -ESHUTDOWN;
		goto out;
	}
	already_enabled = wdog_dd->user_pet_enabled;
	WRITE_ONCE(wdog_dd->user_pet_enabled, enable);
	delay_time = msecs_to_jiffies(wdog_dd->bark_time + 3000);
	if (enable)
		mod_timer(&wdog_dd->user_pet_timer, jiffies + delay_time);
	else if (already_enabled)
		timer_delete_sync(&wdog_dd->user_pet_timer);
	__qcom_wdt_user_pet(wdog_dd);
	ret = count;
out:
	mutex_unlock(&wdog_dd->disable_lock);
	return ret;
}

static DEVICE_ATTR(user_pet_enabled, 0600, qcom_wdt_user_pet_enabled_get,
						qcom_wdt_user_pet_enabled_set);

static ssize_t qcom_wdt_pet_time_get(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);
	int ret;

	ret = scnprintf(buf, PAGE_SIZE, "%d\n", wdog_dd->pet_time);
	return ret;
}

static DEVICE_ATTR(pet_time, 0400, qcom_wdt_pet_time_get, NULL);

static ssize_t wakeup_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d\n", wdog_dd->wakeup_irq_enable);
}
static ssize_t wakeup_enable_store(struct device *dev, struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct msm_watchdog_data *wdog_dd = dev_get_drvdata(dev);
	u8 enable;
	int ret;

	ret = kstrtou8(buf, 10, &enable);
	if (ret || enable > 1) {
		dev_err(wdog_dd->dev, "invalid user input\n");
		return ret ? : -EINVAL;
	}
	if (!wdog_dd)
		return -ENODEV;
	mutex_lock(&wdog_dd->disable_lock);
	if (!smp_load_acquire(&wdog_dd->ready)) {
		mutex_unlock(&wdog_dd->disable_lock);
		return -ENODEV;
	}
	/* echo 1 > wakeup_enable means wakeup irq is enabled */
	wdog_dd->wakeup_irq_enable = enable;
	if (wdog_dd->enabled) {
		u8 val = BIT(EN);

		if (wdog_dd->wakeup_irq_enable)
			val |= BIT(UNMASKED_INT_EN);
		wdog_dd->ops->enable_wdt(val, wdog_dd);
	}
	mutex_unlock(&wdog_dd->disable_lock);
	return count;
}
static DEVICE_ATTR_ADMIN_RW(wakeup_enable);

static void qcom_wdt_keep_alive_response(void *info)
{
	struct msm_watchdog_data *wdog_dd = info;
	int cpu = smp_processor_id();

	cpumask_set_cpu(cpu, &wdog_dd->alive_mask);
	wdog_dd->ping_end[cpu] = sched_clock();
	/* Make sure alive mask is cleared and set in order */
	smp_mb();
}

/*
 * If this function does not return, it implies one of the
 * other cpu's is not responsive.
 */
static void qcom_wdt_ping_other_cpus(struct msm_watchdog_data *wdog_dd)
{
	int cpu;

	cpumask_clear(&wdog_dd->alive_mask);
	/* Make sure alive mask is cleared and set in order */
	smp_mb();
	cpus_read_lock();
	for_each_cpu(cpu, cpu_online_mask) {
		if (!READ_ONCE(wdog_dd->cpu_idle_pc_state[cpu])) {
			wdog_dd->ping_start[cpu] = sched_clock();
			smp_call_function_single(cpu,
						 qcom_wdt_keep_alive_response,
						 wdog_dd, 1);
		}
	}
	cpus_read_unlock();
}

static void qcom_wdt_pet_task_wakeup(struct timer_list *t)
{
	struct msm_watchdog_data *wdog_dd =
		timer_container_of(wdog_dd, t, pet_timer);
	WRITE_ONCE(wdog_dd->timer_expired, true);
	wdog_dd->timer_fired = sched_clock();
	wake_up(&wdog_dd->pet_complete);
}
static void qcom_wdt_user_pet_bite(struct timer_list *t)
{
	struct msm_watchdog_data *wdog_dd =
		timer_container_of(wdog_dd, t, user_pet_timer);
	if (!wdog_dd->user_pet_complete) {
		dev_info(wdog_dd->dev, "QCOM Apps Watchdog user pet timeout!\n");
		qcom_wdt_trigger_bite();
	}
}

static __ref int qcom_wdt_kthread(void *arg)
{
	struct msm_watchdog_data *wdog_dd = arg;
	unsigned long delay_time;
	struct sched_param param = {.sched_priority = MAX_RT_PRIO - 1};
	int ret, cpu;

	sched_setscheduler(current, SCHED_FIFO, &param);
	while (!kthread_should_stop()) {
		ret = wait_event_interruptible(wdog_dd->pet_complete,
			READ_ONCE(wdog_dd->timer_expired) || READ_ONCE(wdog_dd->stopping) ||
			kthread_should_stop());
		if (READ_ONCE(wdog_dd->stopping) || kthread_should_stop())
			break;
		if (ret)
			continue;
		wdog_dd->thread_start = sched_clock();
		for_each_cpu(cpu, cpu_present_mask)
			wdog_dd->ping_start[cpu] = wdog_dd->ping_end[cpu] = 0;
		if (wdog_dd->do_ipi_ping)
			qcom_wdt_ping_other_cpus(wdog_dd);
		do {
			ret = wait_event_interruptible(wdog_dd->pet_complete,
				READ_ONCE(wdog_dd->user_pet_complete) ||
				READ_ONCE(wdog_dd->stopping) || kthread_should_stop());
		} while (ret && !kthread_should_stop());
		if (READ_ONCE(wdog_dd->stopping) || kthread_should_stop())
			break;
		WRITE_ONCE(wdog_dd->timer_expired, false);
		WRITE_ONCE(wdog_dd->user_pet_complete, !READ_ONCE(wdog_dd->user_pet_enabled));
		mutex_lock(&wdog_dd->disable_lock);
		if (wdog_dd->enabled) {
			delay_time = msecs_to_jiffies(wdog_dd->pet_time);
			wdog_dd->ops->reset_wdt(wdog_dd);
			wdog_dd->last_pet = sched_clock();
			spin_lock(&wdog_dd->freeze_lock);
			if (!wdog_dd->freeze_in_progress && !wdog_dd->stopping)
				mod_timer(&wdog_dd->pet_timer, jiffies + delay_time);
			spin_unlock(&wdog_dd->freeze_lock);
		}
		mutex_unlock(&wdog_dd->disable_lock);
		record_irq_count();
	}
	return 0;
}

static int qcom_wdt_cpu_pm_notify(struct notifier_block *this,
			      unsigned long action, void *v)
{
	struct msm_watchdog_data *wdog_dd = container_of(this,
				struct msm_watchdog_data, wdog_cpu_pm_nb);
	int cpu;

	cpu = raw_smp_processor_id();

	switch (action) {
	case CPU_PM_ENTER:
		WRITE_ONCE(wdog_dd->cpu_idle_pc_state[cpu], 1);
		break;
	case CPU_PM_ENTER_FAILED:
	case CPU_PM_EXIT:
		WRITE_ONCE(wdog_dd->cpu_idle_pc_state[cpu], 0);
		break;
	}

	return NOTIFY_OK;
}

/**
 * qcom_wdt_remove() - Removes the watchdog and stops it's kthread.
 *
 *  @pdev: watchdog platform_device
 *
 *  Upon the removal of the module all watchdog data along with the kthread
 *  will be cleaned up and the watchdog device will be removed from memory.
 *
 */
void qcom_wdt_remove(struct platform_device *pdev)
{
	struct msm_watchdog_data *wdog_dd;

	mutex_lock(&wdog_registration_lock);
	wdog_dd = platform_get_drvdata(pdev);
	if (!wdog_dd)
		goto out;
	smp_store_release(&wdog_dd->ready, false);
	if (wdog_dd->sysfs_created) {
		sysfs_remove_group(&wdog_dd->dev->kobj, &qcom_wdt_attr_group);
		wdog_dd->sysfs_created = false;
	}
	mutex_lock(&wdog_dd->disable_lock);
	WRITE_ONCE(wdog_dd->stopping, true);
	qcom_wdt_disable(wdog_dd);
	mutex_unlock(&wdog_dd->disable_lock);
	wake_up_all(&wdog_dd->pet_complete);
	if (!IS_ERR_OR_NULL(wdog_dd->watchdog_task)) {
		kthread_stop(wdog_dd->watchdog_task);
		wdog_dd->watchdog_task = NULL;
	}
	qcom_wdt_remove_dump(wdog_dd);
	wdog_data = NULL;
	platform_set_drvdata(pdev, NULL);
	/* The OF/platform IRQ mapping is not owned by this driver. */
out:
	mutex_unlock(&wdog_registration_lock);
}
EXPORT_SYMBOL(qcom_wdt_remove);

/**
 * qcom_wdt_trigger_bite - Executes a watchdog bite.
 *
 * Return: function will not return, to allow for the bite to occur
 */
void qcom_wdt_trigger_bite(void)
{
	if (!wdog_data)
		return;

	qcom_sdei_shared_reset();
	compute_irq_count();
	dev_err(wdog_data->dev, "Causing a QCOM Apps Watchdog bite!\n");
	wdog_data->ops->show_wdt_status(wdog_data);
	wdog_data->ops->set_bite_time(1, wdog_data);
	wdog_data->ops->reset_wdt(wdog_data);
	/* Delay to make sure bite occurs */
	mdelay(10000);
	/*
	 * This function induces the non-secure bite and control
	 * should not return to the calling function. Non-secure
	 * bite interrupt is affined to all the cores and it may
	 * not be handled by the same cores which configured
	 * non-secure bite. So add forever loop here.
	 */
	while (1)
		udelay(1);

}
EXPORT_SYMBOL(qcom_wdt_trigger_bite);

static irqreturn_t qcom_wdt_bark_handler(int irq, void *dev_id)
{
	struct msm_watchdog_data *wdog_dd = dev_id;
	unsigned long nanosec_rem;
	unsigned long long t = sched_clock();
	unsigned long long tp = wdog_dd->last_pet;

	nanosec_rem = do_div(t, 1000000000);
	dev_err(wdog_dd->dev, "QCOM Apps Watchdog bark! Now = %lu.%06lu\n",
			(unsigned long) t, nanosec_rem / 1000);

	nanosec_rem = do_div(tp, 1000000000);
	dev_err(wdog_dd->dev, "QCOM Apps Watchdog last pet at %lu.%06lu\n",
			(unsigned long) tp, nanosec_rem / 1000);
	if (wdog_dd->do_ipi_ping)
		qcom_wdt_dump_cpu_alive_mask(wdog_dd);

	if (wdog_dd->freeze_in_progress)
		dev_err(wdog_dd->dev, "Suspend in progress\n");

	md_dump_process();
	qcom_wdt_trigger_bite();

	return IRQ_HANDLED;
}

static irqreturn_t qcom_wdt_ppi_bark(int irq, void *dev_id_percpu)
{
	void *dev_id = raw_cpu_ptr((void __percpu *)dev_id_percpu);
	struct msm_watchdog_data *wdog_dd = *((struct msm_watchdog_data **)dev_id);

	return qcom_wdt_bark_handler(irq, wdog_dd);
}

static struct attribute *qcom_wdt_attrs[] = {
	&dev_attr_disable.attr, &dev_attr_wakeup_enable.attr,
	&dev_attr_pet_time.attr, &dev_attr_user_pet_enabled.attr, NULL,
};

static umode_t qcom_wdt_attr_visible(struct kobject *kobj, struct attribute *attr, int index)
{
	if (!QCOM_WATCHDOG_USERSPACE_PET &&
	    (attr == &dev_attr_pet_time.attr || attr == &dev_attr_user_pet_enabled.attr))
		return 0;
	return attr->mode;
}

static const struct attribute_group qcom_wdt_attr_group = {
	.attrs = qcom_wdt_attrs,
	.is_visible = qcom_wdt_attr_visible,
};

static int qcom_wdt_init_sysfs(struct msm_watchdog_data *wdog_dd)
{
	return sysfs_create_group(&wdog_dd->dev->kobj, &qcom_wdt_attr_group);
}

static int qcom_wdt_init(struct msm_watchdog_data *wdog_dd,
			struct platform_device *pdev)
{
	unsigned long delay_time = msecs_to_jiffies(wdog_dd->pet_time);
	u32 val = BIT(EN);
	int ret, cpu;

	mutex_init(&wdog_dd->disable_lock);
	init_waitqueue_head(&wdog_dd->pet_complete);
	spin_lock_init(&wdog_dd->freeze_lock);
	WRITE_ONCE(wdog_dd->user_pet_complete, true);
	timer_setup(&wdog_dd->pet_timer, qcom_wdt_pet_task_wakeup, 0);
	timer_setup(&wdog_dd->user_pet_timer, qcom_wdt_user_pet_bite, 0);
	wdog_dd->timers_initialized = true;
	if (wdog_dd->irq_ppi) {
		wdog_dd->wdog_cpu_dd = alloc_percpu(struct msm_watchdog_data *);
		if (!wdog_dd->wdog_cpu_dd) {
			ret = -ENOMEM;
			goto fail;
		}
		for_each_possible_cpu(cpu)
			*per_cpu_ptr(wdog_dd->wdog_cpu_dd, cpu) = wdog_dd;
		ret = request_percpu_irq(wdog_dd->bark_irq, qcom_wdt_ppi_bark,
				"apps_wdog_bark", wdog_dd->wdog_cpu_dd);
	} else {
		ret = devm_request_irq(wdog_dd->dev, wdog_dd->bark_irq,
			qcom_wdt_bark_handler,
			irq_get_trigger_type(wdog_dd->bark_irq) | IRQF_NO_SUSPEND,
			"apps_wdog_bark", wdog_dd);
	}
	if (ret)
		goto fail;
	wdog_dd->irq_registered = true;
	ret = register_pm_notifier(&qcom_wdt_notif_block);
	if (ret)
		goto fail;
	wdog_dd->pm_registered = true;
	ret = wdog_dd->ops->set_bark_time(wdog_dd->bark_time, wdog_dd);
	if (ret)
		goto fail;
	ret = wdog_dd->ops->set_bite_time(wdog_dd->bark_time + 3000, wdog_dd);
	if (ret)
		goto fail;
	wdog_dd->panic_blk.priority = INT_MAX - 1;
	wdog_dd->panic_blk.notifier_call = qcom_wdt_panic_handler;
	ret = atomic_notifier_chain_register(&panic_notifier_list, &wdog_dd->panic_blk);
	if (ret)
		goto fail;
	wdog_dd->panic_registered = true;
	ret = qcom_wdt_register_die_notifier(wdog_dd);
	if (ret)
		goto fail;
	wdog_dd->restart_blk.priority = 255;
	wdog_dd->restart_blk.notifier_call = restart_wdog_handler;
	ret = register_restart_handler(&wdog_dd->restart_blk);
	if (ret)
		goto fail;
	wdog_dd->restart_registered = true;
	if (!IPI_CORES_IN_LPM) {
		wdog_dd->wdog_cpu_pm_nb.notifier_call = qcom_wdt_cpu_pm_notify;
		ret = cpu_pm_register_notifier(&wdog_dd->wdog_cpu_pm_nb);
		if (ret)
			goto fail;
		wdog_dd->cpu_pm_registered = true;
	}
	if (wdog_dd->wakeup_irq_enable)
		val |= BIT(UNMASKED_INT_EN);
	ret = wdog_dd->ops->enable_wdt(val, wdog_dd);
	if (ret)
		goto fail;
	wdog_dd->enabled = true;
	ret = wdog_dd->ops->reset_wdt(wdog_dd);
	if (ret)
		goto fail;
	wdog_dd->last_pet = sched_clock();
	ret = qcom_wdt_init_sysfs(wdog_dd);
	if (ret)
		goto fail;
	wdog_dd->sysfs_created = true;
	if (wdog_dd->irq_ppi) {
		enable_percpu_irq(wdog_dd->bark_irq, 0);
		wdog_dd->irq_enabled = true;
	}
	smp_store_release(&wdog_dd->ready, true);
	wake_up_process(wdog_dd->watchdog_task);
	mod_timer(&wdog_dd->pet_timer, jiffies + delay_time);
	return 0;
fail:
	mutex_lock(&wdog_dd->disable_lock);
	qcom_wdt_disable(wdog_dd);
	mutex_unlock(&wdog_dd->disable_lock);
	return ret;
}

static void qcom_wdt_dump_pdata(struct msm_watchdog_data *pdata)
{
	dev_dbg(pdata->dev, "wdog bark_time %d", pdata->bark_time);
	dev_dbg(pdata->dev, "wdog pet_time %d", pdata->pet_time);
	dev_dbg(pdata->dev, "wdog perform ipi ping %d", pdata->do_ipi_ping);
	dev_dbg(pdata->dev, "wdog base address is 0x%lx\n", (unsigned long)
								pdata->base);
}

static int qcom_wdt_dt_to_pdata(struct platform_device *pdev,
				struct msm_watchdog_data *pdata)
{
	int irq = platform_get_irq(pdev, 0);

	if (irq < 0)
		return irq;
	pdata->bark_irq = irq;
	pdata->irq_ppi = irq_is_percpu(pdata->bark_irq);
	pdata->bark_time = QCOM_WATCHDOG_BARK_TIME;
	pdata->pet_time = QCOM_WATCHDOG_PET_TIME;
	pdata->do_ipi_ping = QCOM_WATCHDOG_IPI_PING;
	pdata->wakeup_irq_enable = QCOM_WATCHDOG_WAKEUP_ENABLE;
	of_property_read_u32(pdev->dev.of_node, "qcom,bark-time", &pdata->bark_time);
	of_property_read_u32(pdev->dev.of_node, "qcom,pet-time", &pdata->pet_time);
	if (!pdata->pet_time || pdata->pet_time >= pdata->bark_time ||
	    pdata->bark_time > U32_MAX - 3000)
		return -EINVAL;
	qcom_wdt_dump_pdata(pdata);
	return 0;
}

/**
 *  qcom_wdt_register() - Creates QCOM Apps watchdog device.
 *
 *  @pdev: watchdog platform_device
 *  @ops:  watchdog operations
 *
 *  All QCOM Apps watchdogs should be created the same way, this acts
 *  as a framework for this purpose.
 *
 *  0 on success, negative errno on failure.
 */
int qcom_wdt_register(struct platform_device *pdev,
			struct msm_watchdog_data *wdog_dd,
			char *wdog_dd_name)
{
	int ret;

	if (!pdev || !wdog_dd || !wdog_dd_name || !wdog_dd->ops ||
	    !wdog_dd->ops->set_bark_time || !wdog_dd->ops->set_bite_time ||
	    !wdog_dd->ops->reset_wdt || !wdog_dd->ops->enable_wdt ||
	    !wdog_dd->ops->disable_wdt || !wdog_dd->ops->show_wdt_status) {
		pr_err("wdt_register input incorrect\n");
		return -EINVAL;
	}

	ret = qcom_wdt_dt_to_pdata(pdev, wdog_dd);
	if (ret)
		return ret;
	mutex_lock(&wdog_registration_lock);
	if (wdog_data) {
		mutex_unlock(&wdog_registration_lock);
		return -EBUSY;
	}
	wdog_data = wdog_dd;
	wdog_dd->dev = &pdev->dev;
	platform_set_drvdata(pdev, wdog_dd);
	cpumask_clear(&wdog_dd->alive_mask);
	wdog_dd->watchdog_task = kthread_create(qcom_wdt_kthread, wdog_dd,
						wdog_dd_name);
	if (IS_ERR(wdog_dd->watchdog_task)) {
		ret = PTR_ERR(wdog_dd->watchdog_task);
		goto err;
	}
	ret = qcom_wdt_init(wdog_dd, pdev);
	if (ret) {
		kthread_stop(wdog_dd->watchdog_task);
		wdog_dd->watchdog_task = NULL;
		goto err;
	}

	qcom_wdt_add_dump(wdog_dd);

	mutex_unlock(&wdog_registration_lock);
	return 0;
err:
	if (IS_ERR(wdog_dd->watchdog_task))
		wdog_dd->watchdog_task = NULL;
	wdog_data = NULL;
	platform_set_drvdata(pdev, NULL);
	mutex_unlock(&wdog_registration_lock);
	return ret;
}
EXPORT_SYMBOL(qcom_wdt_register);

MODULE_DESCRIPTION("QCOM Watchdog Driver Core");
MODULE_LICENSE("GPL");
