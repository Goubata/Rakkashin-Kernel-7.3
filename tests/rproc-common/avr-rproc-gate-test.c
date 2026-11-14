// SPDX-License-Identifier: GPL-2.0-only
/* Actual common source compiled separately; the production common module is
 * not loaded in this guest. Disabled native transport APIs must fail startup.
 */
#include <kunit/test.h>
#include "../../ports/qcom-rproc-common/qcom_common.c"
static void missing_glink_rejects_start(struct kunit *test)
{
	struct qcom_rproc_glink glink = {};
	if (IS_ENABLED(CONFIG_RPMSG_QCOM_GLINK_SMEM)) {
		kunit_skip(test, "requires native GLINK SMEM disabled configuration");
		return;
	}
	KUNIT_EXPECT_EQ(test, glink_subdev_start(&glink.subdev), -EOPNOTSUPP);
	KUNIT_EXPECT_PTR_EQ(test, glink.edge, NULL);
}
static void missing_smd_rejects_start(struct kunit *test)
{
	struct qcom_rproc_subdev smd = {};
	if (IS_ENABLED(CONFIG_RPMSG_QCOM_SMD)) {
		kunit_skip(test, "requires native SMD disabled configuration");
		return;
	}
	KUNIT_EXPECT_EQ(test, smd_subdev_start(&smd.subdev), -EOPNOTSUPP);
	KUNIT_EXPECT_PTR_EQ(test, smd.edge, NULL);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(missing_glink_rejects_start), KUNIT_CASE(missing_smd_rejects_start), {}
};
static struct kunit_suite suite = { .name = "avr-rproc-transport-gate", .test_cases = cases };
kunit_test_suite(suite);
