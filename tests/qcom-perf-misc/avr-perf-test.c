// SPDX-License-Identifier: GPL-2.0-only
/*
 * KUnit: pmu_lib on the phone's qcom,pmu configuration (no pmu-base, long
 * counters, events 0x08/0x11/0x17 + AMU 0x1000) over the QEMU PMUv3 (no AMU):
 * every CPU exposes the configured PMU events and cycle counts advance, as memlat/bwmon
 * read them on the phone.
 */
#include <kunit/test.h>
#include <linux/cpu.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <soc/qcom/pmu_lib.h>

static void pmu_counts_advance(struct kunit *test)
{
	/* QEMU TCG (no icount) counts cycles but not INST_RETIRED/L2 refills,
	 * so all configured events must be available and cycles must advance. */
	static const u32 ev[] = { 0x11 };	/* cycles */
	static const u32 cfg[] = { 0x08, 0x11, 0x17 };
	unsigned int cpu, i;
	unsigned long wait_end = jiffies + 5 * HZ;

	/*
	 * Built like the phone (QTI SCMI vendor protocol), pmu_lib defers its probe
	 * until the qcom SCMI device exists; the fake CPUCP firmware loads just
	 * before this test, so wait for the deferred probe instead of racing it.
	 */
	while (qcom_pmu_event_supported(0x11, 0) == -EPROBE_DEFER &&
	       time_before(jiffies, wait_end))
		msleep(50);

	for_each_online_cpu(cpu) {
		for (i = 0; i < ARRAY_SIZE(cfg); i++)
			KUNIT_EXPECT_EQ_MSG(test, qcom_pmu_event_supported(cfg[i], cpu), 0,
					    "cpu%u event %#x", cpu, cfg[i]);
		for (i = 0; i < ARRAY_SIZE(ev); i++) {
			u64 a = 0, b = 0;
			unsigned long end;

			KUNIT_ASSERT_EQ_MSG(test, qcom_pmu_event_supported(ev[i], cpu), 0,
					    "cpu%u event %#x", cpu, ev[i]);
			KUNIT_EXPECT_EQ(test, qcom_pmu_read(cpu, ev[i], &a), 0);
			end = jiffies + HZ / 10;
			while (time_before(jiffies, end))
				cpu_relax();
			msleep(20);
			KUNIT_EXPECT_EQ(test, qcom_pmu_read(cpu, ev[i], &b), 0);
			KUNIT_EXPECT_GT_MSG(test, b, a, "cpu%u event %#x did not advance", cpu, ev[i]);
		}
	}
	/* an event the phone table does not configure is reported unsupported */
	KUNIT_EXPECT_NE(test, qcom_pmu_event_supported(0x24, 0), 0);
}

static struct kunit_case cases[] = { KUNIT_CASE(pmu_counts_advance), {} };
static struct kunit_suite suite = { .name = "avr_qcom_pmu_lib", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
