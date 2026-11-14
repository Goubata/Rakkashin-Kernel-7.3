// SPDX-License-Identifier: GPL-2.0-only
/* Actual common source compiled separately; the production common module is
 * not loaded in this guest. Unavailable or invalid transports must fail startup.
 */
#include <kunit/test.h>
#include "../../ports/qcom-rproc-common/qcom_common.c"
static void glink_transport_error_is_propagated(struct kunit *test)
{
	struct qcom_rproc_glink glink = {};
	KUNIT_ASSERT_TRUE(test, IS_ENABLED(CONFIG_RPMSG_QCOM_GLINK_SMEM));
	/* Real provider rejects missing parent/DT data before touching hardware. */
	KUNIT_EXPECT_EQ(test, glink_subdev_start(&glink.subdev), -EINVAL);
	KUNIT_EXPECT_EQ(test, PTR_ERR(glink.edge), -EINVAL);
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
	KUNIT_CASE(glink_transport_error_is_propagated), KUNIT_CASE(missing_smd_rejects_start), {}
};
static struct kunit_suite suite = { .name = "avr-rproc-transport-gate", .test_cases = cases };
kunit_test_suite(suite);
