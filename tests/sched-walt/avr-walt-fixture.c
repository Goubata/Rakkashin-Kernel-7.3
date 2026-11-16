// SPDX-License-Identifier: GPL-2.0-only
/*
 * Private SoC fixture for running the real sched_walt module in QEMU: a cpufreq
 * driver with the AVR phone tables (policy0 little = CPUs 0-1, policy6 big =
 * CPUs 2-3 here) plus an energy model per policy so the native perf-domain/EAS
 * path and arch_topology capacity update run exactly as on the phone. Power
 * numbers are synthetic (monotonic C*f*V^2); they are not phone measurements.
 */
#include <linux/cpu.h>
#include <linux/cpufreq.h>
#include <linux/energy_model.h>
#include <linux/module.h>

static const unsigned int little_khz[] = {
	384000, 556800, 710400, 806400, 902400, 1036800, 1152000, 1267200, 1401600, 1555200,
	1651200, 1747200, 1900800, 2016000, 2188800, 2265600, 2361600, 2476800, 2611200,
	2707200, 2841600, 3033600, 3129600, 3321600,
};
static const unsigned int big_khz[] = {
	384000, 556800, 806400, 921600, 1017600, 1152000, 1248000, 1382400, 1497600, 1651200,
	1862400, 1996800, 2112000, 2227200, 2476800, 2688000, 2841600, 2995200, 3091200,
	3244800, 3436800,
};

static struct cpufreq_frequency_table tl[ARRAY_SIZE(little_khz) + 1];
static struct cpufreq_frequency_table tb[ARRAY_SIZE(big_khz) + 1];
static unsigned int cur[NR_CPUS];

static int fx_active_power(struct device *dev, unsigned long *uw, unsigned long *khz)
{
	const unsigned int *t;
	int n, i, cpu = dev->id;
	unsigned long mhz, mv;

	t = cpu < 2 ? little_khz : big_khz;
	n = cpu < 2 ? ARRAY_SIZE(little_khz) : ARRAY_SIZE(big_khz);
	for (i = 0; i < n - 1 && t[i] < *khz; i++)
		;
	*khz = t[i];
	mhz = t[i] / 1000;
	mv = 500 + mhz / 8;			/* synthetic voltage curve */
	*uw = (cpu < 2 ? 60 : 160) * mhz * mv * mv / 1000000;
	return 0;
}

static int fx_init(struct cpufreq_policy *policy)
{
	int first = policy->cpu < 2 ? 0 : 2;

	cpumask_set_cpu(first, policy->cpus);
	cpumask_set_cpu(first + 1, policy->cpus);
	policy->freq_table = first ? tb : tl;
	policy->cpuinfo.transition_latency = 20000;
	cur[policy->cpu] = policy->freq_table[0].frequency;
	return 0;
}

static void fx_ready(struct cpufreq_policy *policy)
{
	struct em_data_callback cb = EM_DATA_CB(fx_active_power);
	int n = policy->cpu < 2 ? ARRAY_SIZE(little_khz) : ARRAY_SIZE(big_khz);

	em_dev_register_perf_domain(get_cpu_device(policy->cpu), n, &cb,
				    policy->related_cpus, true);
}

static int fx_target_index(struct cpufreq_policy *policy, unsigned int index)
{
	int cpu;

	for_each_cpu(cpu, policy->related_cpus)
		cur[cpu] = policy->freq_table[index].frequency;
	return 0;
}

static unsigned int fx_get(unsigned int cpu)
{
	return cur[cpu];
}

static struct cpufreq_driver fx_driver = {
	.name = "avr-walt-fixture",
	.flags = CPUFREQ_NEED_INITIAL_FREQ_CHECK | CPUFREQ_HAVE_GOVERNOR_PER_POLICY,	/* as scmi-cpufreq */
	.init = fx_init,
	.ready = fx_ready,
	.verify = cpufreq_generic_frequency_table_verify,
	.target_index = fx_target_index,
	.get = fx_get,
};

static int __init fx_mod_init(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(little_khz); i++)
		tl[i].frequency = little_khz[i];
	tl[i].frequency = CPUFREQ_TABLE_END;
	for (i = 0; i < ARRAY_SIZE(big_khz); i++)
		tb[i].frequency = big_khz[i];
	tb[i].frequency = CPUFREQ_TABLE_END;
	return cpufreq_register_driver(&fx_driver);
}
module_init(fx_mod_init);

static void __exit fx_mod_exit(void)
{
	cpufreq_unregister_driver(&fx_driver);
}
module_exit(fx_mod_exit);
MODULE_LICENSE("GPL");
