// SPDX-License-Identifier: GPL-2.0-only
/*
 * KUnit: Qualcomm SCMI vendor protocol (0x80) and qcom_scmi_client on the real
 * 7.3 SCMI core, reached exactly as vendor users (memlat, dcvs, cpucp_fast)
 * do: get_qcom_scmi_device() -> devm_protocol_get(0x80) -> vendor ops.
 */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/qcom_scmi_vendor.h>
#include <linux/scmi_protocol.h>

static void vendor_roundtrip(struct kunit *test)
{
	struct scmi_device *sdev = get_qcom_scmi_device();
	const struct qcom_scmi_vendor_ops *ops;
	struct scmi_protocol_handle *ph;
	u32 set[4] = { 0x11, 0x2222, 0x333333, 0x44444444 }, get[4] = {};
	u64 algo = 0x4d454d4c4154ULL;	/* "MEMLAT" */

	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(sdev));
	KUNIT_EXPECT_STREQ(test, sdev->handle->version->vendor_id, "Qualcomm");
	KUNIT_EXPECT_STREQ(test, sdev->handle->version->sub_vendor_id, "CPUCP");
	ops = sdev->handle->devm_protocol_get(sdev, QCOM_SCMI_VENDOR_PROTOCOL, &ph);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(ops));
	KUNIT_EXPECT_EQ(test, ops->set_param(ph, set, algo, 7, sizeof(set)), 0);
	KUNIT_EXPECT_EQ(test, ops->get_param(ph, get, algo, 7, 0, sizeof(get)), 0);
	KUNIT_EXPECT_MEMEQ(test, get, set, sizeof(set));
	KUNIT_EXPECT_EQ(test, ops->start_activity(ph, set, algo, 1, 4), 0);
	KUNIT_EXPECT_EQ(test, ops->stop_activity(ph, set, algo, 1, 4), 0);
	/* PDP0 hosts perf only: no vendor protocol device there */
	KUNIT_EXPECT_TRUE(test, IS_ERR(platform_get_qcom_scmi_device(QCOM_PLAT_PDP0)));
}

static struct kunit_case cases[] = { KUNIT_CASE(vendor_roundtrip), {} };
static struct kunit_suite suite = { .name = "avr_qcom_scmi_vendor", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
