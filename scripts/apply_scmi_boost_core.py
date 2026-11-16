#!/usr/bin/env python3
"""Keep android16-6.12 scmi-cpufreq boost default on 7.3.

GKI 6.12 scmi_cpufreq_init(): when the OPP table has boost (turbo, above the
sustained level) frequencies it set scmi_cpufreq_driver.boost_enabled = true, so
the phone boots with /sys/devices/system/cpu/cpufreq/boost = 1 and the gold
cluster at 3.80 GHz. 7.x moved boost to per-policy state seeded from the
driver flag and dropped that line, so the same firmware boots with boost off
(gold capped at 3.44 GHz). Restore the 6.12 default; userspace can still turn
boost off through the same sysfs file.

Also fixes cpufreq_boost_set_sw() on 7.3: turning boost off kept the boost
cpuinfo.max_freq (cpufreq_frequency_table_cpuinfo never lowers it), so
/sys/devices/system/cpu/cpufreq/boost = 0 had no effect.
"""
from pathlib import Path
import sys
SRC = Path(sys.argv[1] if len(sys.argv) > 1 else Path(__file__).resolve().parents[2] / 'avr-upstream-audit/android-7.3')
p = SRC / 'drivers/cpufreq/scmi-cpufreq.c'
s = p.read_text()
old = '''	policy->driver_data = priv;
	policy->freq_table = freq_table;
'''
new = '''	policy->driver_data = priv;
	policy->freq_table = freq_table;

	/* ANDROID: 6.12 GKI parity: boost (turbo OPPs) is on by default */
	{
		struct cpufreq_frequency_table *pos;

		cpufreq_for_each_valid_entry(pos, freq_table)
			if (pos->flags & CPUFREQ_BOOST_FREQ) {
				scmi_cpufreq_driver.boost_enabled = true;
				break;
			}
	}
'''
if new not in s:
    if s.count(old) != 1:
        raise SystemExit('anchor')
    s = s.replace(old, new, 1)
    p.write_text(s)
c = SRC / 'drivers/cpufreq/cpufreq.c'
t = c.read_text()
old = '''	if (!policy->freq_table)
		return -ENXIO;

	ret = cpufreq_frequency_table_cpuinfo(policy);'''
new = '''	if (!policy->freq_table)
		return -ENXIO;

	/*
	 * ANDROID: cpufreq_frequency_table_cpuinfo() keeps a cpuinfo.max_freq
	 * that is above the table maximum ("driver set its own"). For this
	 * table-driven helper that is always the previous boost maximum, so
	 * turning boost off never lowered the limit (found on the AVR SCMI
	 * policies). Recompute both bounds from the table.
	 */
	policy->cpuinfo.max_freq = 0;
	ret = cpufreq_frequency_table_cpuinfo(policy);'''
if new not in t:
    if t.count(old) != 1:
        raise SystemExit('cpufreq.c anchor')
    c.write_text(t.replace(old, new, 1))
print('applied scmi-cpufreq boost default + boost_set_sw fix to', SRC)
