// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include "../../ports/kgsl/avr-adrenoboost-policy.h"

static void inherited_floor_respects_new_cap(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 2, 1, 3, 7, true, true), 3);
}
static void held_floor_respects_new_cap(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 2, 1, 4, 7, false, true), 4);
}
static void no_headroom_drops_boost(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 2, 1, 5, 5, true, true), -1);
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 2, 1, 6, 5, true, true), -1);
}
static void disabled_drops_previous_floor(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 0, 1, 0, 7, true, true), -1);
}
static void expired_hold_drops_floor(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 3, 1, 0, 7, false, false), -1);
}
static void ordinary_headroom(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 2, -1, 0, 7, true, false), 4);
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_select(6, 3, 2, 0, 7, true, true), 2);
}
static void all_small_level_combinations(struct kunit *test)
{
	int active, previous, cap, floor, request;
	unsigned int level;

	for (active = 0; active < 8; active++)
		for (previous = -1; previous < 8; previous++)
			for (cap = 0; cap < 8; cap++)
				for (floor = 0; floor < 8; floor++)
					for (level = 0; level <= 3; level++) {
						request = avr_adrenoboost_select(active, level, previous,
							cap, floor, true, true);
						KUNIT_ASSERT_TRUE(test, request == -1 ||
							(request >= cap && request < floor));
						if (!level)
							KUNIT_ASSERT_EQ(test, request, -1);
					}
}
static void worker_rechecks_latest_limits(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_clamp(1, 3, 7), 3);
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_clamp(4, 2, 4), -1);
	KUNIT_EXPECT_EQ(test, avr_adrenoboost_clamp(-1, 3, 7), -1);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(inherited_floor_respects_new_cap), KUNIT_CASE(held_floor_respects_new_cap),
	KUNIT_CASE(no_headroom_drops_boost), KUNIT_CASE(disabled_drops_previous_floor),
	KUNIT_CASE(expired_hold_drops_floor), KUNIT_CASE(ordinary_headroom),
	KUNIT_CASE(all_small_level_combinations), KUNIT_CASE(worker_rechecks_latest_limits), {}
};
static struct kunit_suite suite = { .name = "avr-adrenoboost-policy", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Production Adreno Boost cap selection tests without GPU hardware");
