// SPDX-License-Identifier: GPL-2.0-only

/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#define pr_fmt(fmt) "qcom_sdei: " fmt

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/soc/qcom/qcom_sdei.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/arm_sdei.h>
#include <linux/sched/debug.h>
#include <linux/arm-smccc.h>
#include <linux/delay.h>
#include <uapi/linux/psci.h>

#define SDEI_EVENT_STANDARD		0x0
#define SDEI_EVENT_WDG_BITE		0x40000000

static_assert(SDEI_1_0_FN_SDEI_EVENT_SIGNAL == 0xC400002F);

static DEFINE_PER_CPU(u64, cpu_mpidr);

static int sdei_standard_cb(u32 event, struct pt_regs *regs, void *arg)
{
	int cpu = smp_processor_id();

	pr_crit("SDEI event 0x%x triggered on cpu %d\n", event, cpu);

	show_regs(regs);

	return 0;
}

static int psci_system_reset(void)
{
	struct arm_smccc_res res = { 0 };

	arm_smccc_1_1_hvc(PSCI_0_2_FN_SYSTEM_RESET, &res);

	return res.a0;
}

static int sdei_shared_reset(void)
{
	struct arm_smccc_res res = { 0 };

	arm_smccc_1_1_hvc(SDEI_1_0_FN_SDEI_SHARED_RESET, &res);

	return res.a0;
}

static int qcom_sdei_event_signal(u64 target_pe)
{
	struct arm_smccc_res res = { 0 };

	arm_smccc_1_1_hvc(SDEI_1_0_FN_SDEI_EVENT_SIGNAL, 0, target_pe, &res);

	return res.a0;
}

int qcom_sdei_shared_reset(void)
{
	return sdei_shared_reset();
}
EXPORT_SYMBOL_GPL(qcom_sdei_shared_reset);

static int sdei_wdg_bite_cb(u32 event, struct pt_regs *regs, void *arg)
{
	int cpu;

	pr_crit("SDEI event 0x%x triggered on cpu %d\n", event, smp_processor_id());

	show_regs(regs);

	for_each_possible_cpu(cpu) {
		if (cpu == smp_processor_id())
			continue;
		qcom_sdei_event_signal(per_cpu(cpu_mpidr, cpu));
		mdelay(1);
	}

	psci_system_reset();

	return 0;
}

static void register_cpu_mpidr(void *info)
{
	per_cpu(cpu_mpidr, smp_processor_id()) = read_cpuid_mpidr();
}

/* Native SDEI keeps callback pointers until unregister has succeeded.
 * Pin this module while any registration exists, including failed teardown.
 * A bound provider must be unbound successfully before it can be unloaded. */
static DEFINE_MUTEX(sdei_bind_lock);
static unsigned long registered_events;
static bool event_owner_pinned;
static const u32 qcom_sdei_events[] = { SDEI_EVENT_STANDARD, SDEI_EVENT_WDG_BITE };

static int qcom_sdei_cleanup_events(void)
{
	int i, ret, first_error = 0;

	for (i = ARRAY_SIZE(qcom_sdei_events) - 1; i >= 0; i--) {
		unsigned int retry = 0;

		if (!(registered_events & BIT(i)))
			continue;
		sdei_event_disable(qcom_sdei_events[i]);
		do {
			ret = sdei_event_unregister(qcom_sdei_events[i]);
			if (ret != -EINPROGRESS)
				break;
			msleep(1);
		} while (++retry < 1000);
		if (!ret || ret == -ENOENT)
			registered_events &= ~BIT(i);
		else if (!first_error)
			first_error = ret;
	}
	if (!registered_events && event_owner_pinned) {
		event_owner_pinned = false;
		module_put(THIS_MODULE);
	}
	return first_error;
}

static int qcom_sdei_driver_probe(struct platform_device *pdev)
{
	sdei_event_callback *callbacks[] = { sdei_standard_cb, sdei_wdg_bite_cb };
	int i, ret;

	mutex_lock(&sdei_bind_lock);
	/* Retained registrations may belong to a failed prior probe/remove. */
	if (registered_events) {
		ret = -EBUSY;
		goto out;
	}
	if (!try_module_get(THIS_MODULE)) {
		ret = -ENODEV;
		goto out;
	}
	event_owner_pinned = true;
	on_each_cpu(register_cpu_mpidr, NULL, 1);
	for (i = 0; i < ARRAY_SIZE(qcom_sdei_events); i++) {
		ret = sdei_event_register(qcom_sdei_events[i], callbacks[i], NULL);
		if (ret)
			goto fail;
		registered_events |= BIT(i);
		ret = sdei_event_enable(qcom_sdei_events[i]);
		if (ret)
			goto fail;
	}
	ret = 0;
	goto out;
fail:
	if (qcom_sdei_cleanup_events())
		dev_err(&pdev->dev, "SDEI callbacks retained; provider remains pinned\n");
out:
	mutex_unlock(&sdei_bind_lock);
	return ret;
}

static void qcom_sdei_driver_remove(struct platform_device *pdev)
{
	mutex_lock(&sdei_bind_lock);
	if (qcom_sdei_cleanup_events())
		dev_err(&pdev->dev, "SDEI callbacks retained; provider remains pinned\n");
	mutex_unlock(&sdei_bind_lock);
}

static const struct of_device_id qcom_sdei_of_match[] = {
	{ .compatible = "qcom,sdei" },
	{ }
};
MODULE_DEVICE_TABLE(of, qcom_sdei_of_match);

static struct platform_driver qcom_sdei_driver = {
	.driver = {
		.name = "qcom_sdei",
		.of_match_table = qcom_sdei_of_match,
	},
	.probe = qcom_sdei_driver_probe,
	.remove = qcom_sdei_driver_remove,
};
module_platform_driver(qcom_sdei_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Qualcomm Technologies, Inc. SDEI driver");
