// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/module.h>
#include "qcom_ice_v2.h"
#include "qcom_scm_retry.h"
#include "../../ports/qcom-ice/avr-ice-policy.h"

static int init(struct kunit *test)
{
	/* Tests run only in the diskless guest, never against real SCM. */
	return qcom_scm_is_available() ? -EBUSY : 0;
}
static void set_wire(struct kunit *test)
{
	struct qcom_scm_desc d = qcom_ice_v2_set_desc(16, 0x123456789ULL,
						  100, 0, 0, 0);

	KUNIT_EXPECT_EQ(test, d.svc, 0x10U);
	KUNIT_EXPECT_EQ(test, d.cmd, 5U);
	KUNIT_EXPECT_EQ(test, d.owner, (u32)ARM_SMCCC_OWNER_SIP);
	KUNIT_EXPECT_EQ(test, d.arginfo, 0x86U);
	KUNIT_EXPECT_EQ(test, d.args[0], 16ULL);
	KUNIT_EXPECT_EQ(test, d.args[1], 0x123456789ULL);
	KUNIT_EXPECT_EQ(test, d.args[2], 100ULL);
	KUNIT_EXPECT_EQ(test, d.args[3], 0ULL);
	KUNIT_EXPECT_EQ(test, d.args[4], 0ULL);
	KUNIT_EXPECT_EQ(test, d.args[5], 0ULL);
	/* Other CE and cipher/unit arguments must not be silently discarded. */
	d = qcom_ice_v2_set_desc(3, 0x1000, 64, 3, 8, 1);
	KUNIT_EXPECT_EQ(test, d.args[3], 3ULL);
	KUNIT_EXPECT_EQ(test, d.args[4], 8ULL);
	KUNIT_EXPECT_EQ(test, d.args[5], 1ULL);
}
static void clear_wire(struct kunit *test)
{
	struct qcom_scm_desc d = qcom_ice_v2_clear_desc(16, 1);

	KUNIT_EXPECT_EQ(test, d.cmd, 6U);
	KUNIT_EXPECT_EQ(test, d.svc, 0x10U);
	KUNIT_EXPECT_EQ(test, d.arginfo, 2U);
	KUNIT_EXPECT_EQ(test, d.owner, (u32)ARM_SMCCC_OWNER_SIP);
	KUNIT_EXPECT_EQ(test, d.args[0], 16ULL);
	KUNIT_EXPECT_EQ(test, d.args[1], 1ULL);
}
static void derive_wire(struct kunit *test)
{
	struct qcom_scm_desc d = qcom_ice_v2_derive_desc(0x123400000ULL,
						  100, 0x567800000ULL, 32);

	KUNIT_EXPECT_EQ(test, d.cmd, 7U);
	KUNIT_EXPECT_EQ(test, d.svc, 0x10U);
	KUNIT_EXPECT_EQ(test, d.arginfo, 0x224U);
	KUNIT_EXPECT_EQ(test, d.owner, (u32)ARM_SMCCC_OWNER_SIP);
	KUNIT_EXPECT_EQ(test, d.args[0], 0x123400000ULL);
	KUNIT_EXPECT_EQ(test, d.args[1], 100ULL);
	KUNIT_EXPECT_EQ(test, d.args[2], 0x567800000ULL);
	KUNIT_EXPECT_EQ(test, d.args[3], 32ULL);
}
static void retry_policy(struct kunit *test)
{
	KUNIT_EXPECT_FALSE(test, qcom_scm_retry_busy(false, -12, 0, 20));
	KUNIT_EXPECT_TRUE(test, qcom_scm_retry_busy(true, -12, 0, 20));
	KUNIT_EXPECT_TRUE(test, qcom_scm_retry_busy(true, -12, 20, 20));
	KUNIT_EXPECT_FALSE(test, qcom_scm_retry_busy(true, -12, 21, 20));
	KUNIT_EXPECT_FALSE(test, qcom_scm_retry_busy(true, 0, 0, 20));
	KUNIT_EXPECT_FALSE(test, qcom_scm_retry_busy(true, -1, 0, 20));
}
static void slot_mapping(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(3, false, 2, true, 0x02000000), 3);
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(3, true, 1, true, 0x02000000), 3);
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(3, true, 2, true, 0x02000000), 16);
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(3, true, 2, true, 0x0200ffff), 16);
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(3, true, 2, true, 0x02010000), 6);
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(3, true, 2, false, 0x02000000), 6);
}
static void slot_bounds(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(-1, true, 2, true, 0x02000000), -EINVAL);
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot(INT_MAX, true, 2, true, 0x02000000), -ERANGE);
	KUNIT_EXPECT_EQ(test, avr_ice_hwkm_slot((INT_MAX - 10) / 2, true, 2, true,
					    0x02000000), INT_MAX - 1);
}
static void wrapped_request_ready(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_ice_wrapped_ready(true, false, 100, 8, 3), -EAGAIN);
	KUNIT_EXPECT_EQ(test, avr_ice_wrapped_ready(true, true, 100, 8, 3), 0);
	KUNIT_EXPECT_EQ(test, avr_ice_wrapped_ready(false, false, 100, 8, 3), 0);
	KUNIT_EXPECT_EQ(test, avr_ice_wrapped_ready(true, true, 129, 8, 3), -EINVAL);
	KUNIT_EXPECT_EQ(test, avr_ice_wrapped_ready(true, true, 100, 3, 3), -EINVAL);
	KUNIT_EXPECT_EQ(test, avr_ice_wrapped_ready(true, true, 100, 0, 3), -EINVAL);
	KUNIT_EXPECT_EQ(test, avr_ice_wrapped_ready(true, true, 100, 8, -1), -EINVAL);
}
static void no_scm(struct kunit *test)
{
	u8 key[100] = {}, secret[32];

	KUNIT_EXPECT_FALSE(test, qcom_scm_ice_v2_available());
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_set_key_v2(0, key, sizeof(key), 0, 0, 0), -EPROBE_DEFER);
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_clear_key_v2(0, 0), -EPROBE_DEFER);
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_derive_secret_v2(key, sizeof(key), secret,
						       sizeof(secret)), -EPROBE_DEFER);
}
static void invalid_arguments(struct kunit *test)
{
	u8 key[129] = {}, secret[32];

	KUNIT_EXPECT_EQ(test, qcom_scm_ice_set_key_v2(0, NULL, 100, 0, 0, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_set_key_v2(0, key, 0, 0, 0, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_set_key_v2(0, key, sizeof(key), 0, 0, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_derive_secret_v2(NULL, 100, secret, 32), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_derive_secret_v2(key, 100, NULL, 32), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_scm_ice_derive_secret_v2(key, 100, secret, 31), -EINVAL);
}
static void no_scm_output(struct kunit *test)
{
	u8 key[100] = {}, secret[32];

	memset(secret, 0xa5, sizeof(secret));
	KUNIT_ASSERT_EQ(test, qcom_scm_ice_derive_secret_v2(key, sizeof(key), secret,
						       sizeof(secret)), -EPROBE_DEFER);
	KUNIT_EXPECT_PTR_EQ(test, memchr_inv(secret, 0, sizeof(secret)), NULL);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(set_wire), KUNIT_CASE(clear_wire), KUNIT_CASE(derive_wire),
	KUNIT_CASE(retry_policy), KUNIT_CASE(slot_mapping), KUNIT_CASE(slot_bounds),
	KUNIT_CASE(wrapped_request_ready), KUNIT_CASE(no_scm), KUNIT_CASE(invalid_arguments), KUNIT_CASE(no_scm_output), {}
};
static struct kunit_suite suite = {
	.name = "avr-ice-v2", .init = init, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AVR ICE V2 descriptors, slot policy and no-SCM guards");
