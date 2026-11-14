// SPDX-License-Identifier: GPL-2.0-only
/* Execute real aggregation and native xlate. No RPMh/MMIO device is emulated. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/interconnect-provider.h>
#include "icc-rpmh.h"
#include "drivers/interconnect/qcom/icc-common.h"

struct fixture {
	struct qcom_icc_provider provider;
	struct qcom_icc_node qnode;
	struct icc_node node;
};
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	f->node.data = &f->qnode;
	f->node.provider = &f->provider.provider;
	test->priv = f;
	return 0;
}
static void default_active_and_sleep(struct kunit *test)
{
	struct fixture *f = test->priv;
	u32 avg = 0, peak = 0;
	int i;
	KUNIT_EXPECT_EQ(test, qcom_icc_aggregate(&f->node, 0, 12, 31, &avg, &peak), 0);
	for (i = 0; i < 5; i++) {
		KUNIT_EXPECT_EQ(test, f->qnode.sum_avg[i], i < 3 ? 12ULL : 0ULL);
		KUNIT_EXPECT_EQ(test, f->qnode.max_peak[i], i < 3 ? 31ULL : 0ULL);
	}
	KUNIT_EXPECT_EQ(test, avg, 12U);
	KUNIT_EXPECT_EQ(test, peak, 31U);
}
static void sleep_aggregation(struct kunit *test)
{
	struct fixture *f = test->priv;
	u32 avg = 0, peak = 0;
	qcom_icc_aggregate(&f->node, QCOM_ICC_TAG_SLEEP, 7, 40, &avg, &peak);
	qcom_icc_aggregate(&f->node, QCOM_ICC_TAG_SLEEP, 11, 25, &avg, &peak);
	KUNIT_EXPECT_EQ(test, f->qnode.sum_avg[2], 18ULL);
	KUNIT_EXPECT_EQ(test, f->qnode.max_peak[2], 40ULL);
	KUNIT_EXPECT_EQ(test, f->qnode.sum_avg[0], 0ULL);
	KUNIT_EXPECT_EQ(test, avg, 18U);
	KUNIT_EXPECT_EQ(test, peak, 40U);
}
static void five_power_states(struct kunit *test)
{
	struct fixture *f = test->priv;
	u32 avg = 0, peak = 0;
	KUNIT_EXPECT_EQ(test, QCOM_ICC_NUM_BUCKETS, 5);
	qcom_icc_aggregate(&f->node, QCOM_ICC_TAG_PWR_ST_3, 13, 17, &avg, &peak);
	qcom_icc_aggregate(&f->node, QCOM_ICC_TAG_PWR_ST_4, 19, 23, &avg, &peak);
	KUNIT_EXPECT_EQ(test, f->qnode.sum_avg[3], 13ULL);
	KUNIT_EXPECT_EQ(test, f->qnode.max_peak[4], 23ULL);
	KUNIT_EXPECT_EQ(test, f->qnode.sum_avg[0], 0ULL);
}
static void performance_tag(struct kunit *test)
{
	struct fixture *f = test->priv;
	u32 avg = 0, peak = 0;
	u32 tag = QCOM_ICC_TAG_PWR_ST_4 | QCOM_ICC_TAG_PERF_MODE;
	qcom_icc_aggregate(&f->node, tag, 0, 0, &avg, &peak);
	KUNIT_EXPECT_FALSE(test, f->qnode.perf_mode[4]);
	qcom_icc_aggregate(&f->node, tag, 1, 2, &avg, &peak);
	KUNIT_EXPECT_TRUE(test, f->qnode.perf_mode[4]);
	KUNIT_EXPECT_FALSE(test, f->qnode.perf_mode[0]);
}
static void boot_floor(struct kunit *test)
{
	struct fixture *f = test->priv;
	u32 avg = 0, peak = 0;
	int i;
	f->node.init_avg = 100; f->node.init_peak = 200;
	qcom_icc_aggregate(&f->node, QCOM_ICC_TAG_WAKE, 10, 20, &avg, &peak);
	for (i = 0; i < 5; i++) {
		KUNIT_EXPECT_EQ(test, f->qnode.sum_avg[i], 100ULL);
		KUNIT_EXPECT_EQ(test, f->qnode.max_peak[i], 200ULL);
	}
}
static void clear_previous_vote(struct kunit *test)
{
	struct fixture *f = test->priv;
	u32 avg = 0, peak = 0;
	int i;
	qcom_icc_aggregate(&f->node, 0x1f | QCOM_ICC_TAG_PERF_MODE, 9, 17, &avg, &peak);
	qcom_icc_pre_aggregate(&f->node);
	for (i = 0; i < 5; i++) {
		KUNIT_EXPECT_EQ(test, f->qnode.sum_avg[i], 0ULL);
		KUNIT_EXPECT_EQ(test, f->qnode.max_peak[i], 0ULL);
		KUNIT_EXPECT_FALSE(test, f->qnode.perf_mode[i]);
	}
}
static void scaling_preserves_framework(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->qnode.bw_scale_numerator = 3; f->qnode.bw_scale_denominator = 2;
	f->node.avg_bw = 1000; f->node.peak_bw = 2000;
	KUNIT_EXPECT_EQ(test, qcom_icc_set(&f->node, NULL), 0);
	KUNIT_EXPECT_EQ(test, qcom_icc_set(&f->node, NULL), 0);
	KUNIT_EXPECT_EQ(test, f->node.avg_bw, 1000U);
	KUNIT_EXPECT_EQ(test, f->node.peak_bw, 2000U);
}
static void commit_failure_propagation(struct kunit *test)
{
	struct fixture *f = test->priv;
	/* This token was never registered as a voter; the real registry rejects it. */
	struct bcm_voter *voter = (struct bcm_voter *)f;
	f->provider.voters = &voter; f->provider.num_voters = 1;
	KUNIT_EXPECT_EQ(test, qcom_icc_set(&f->node, NULL), -ENODEV);
}
static void native_translation(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct icc_onecell_data *data;
	struct icc_node_data *translated;
	struct of_phandle_args spec = { .args_count = 2, .args = {0, QCOM_ICC_TAG_PWR_ST_4 | QCOM_ICC_TAG_PERF_MODE} };
	data = kunit_kzalloc(test, struct_size(data, nodes, 1), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, data);
	data->num_nodes = 1; data->nodes[0] = &f->node;
	translated = qcom_icc_xlate_extended(&spec, data);
	KUNIT_ASSERT_FALSE(test, IS_ERR(translated));
	KUNIT_EXPECT_PTR_EQ(test, translated->node, &f->node);
	KUNIT_EXPECT_EQ(test, translated->tag, spec.args[1]);
	kfree(translated);
	spec.args[0] = 1;
	translated = qcom_icc_xlate_extended(&spec, data);
	KUNIT_EXPECT_TRUE(test, IS_ERR(translated));
	if (!IS_ERR(translated))
		kfree(translated);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(default_active_and_sleep), KUNIT_CASE(sleep_aggregation),
	KUNIT_CASE(five_power_states), KUNIT_CASE(performance_tag),
	KUNIT_CASE(boot_floor), KUNIT_CASE(clear_previous_vote),
	KUNIT_CASE(scaling_preserves_framework), KUNIT_CASE(commit_failure_propagation),
	KUNIT_CASE(native_translation), {}
};
static struct kunit_suite suite = { .name = "avr-interconnect", .init = setup, .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
