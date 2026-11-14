// SPDX-License-Identifier: GPL-2.0-only
/* Native regulator framework + actual vendor logical regulator, guest only. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/regulator/consumer.h>

static int get_rail(struct kunit *test)
{
	struct regulator *rail = regulator_get(NULL, "avr_qemu_rail");
	if (IS_ERR(rail))
		return PTR_ERR(rail);
	test->priv = rail;
	return 0;
}
static void put_rail(struct kunit *test) { regulator_put(test->priv); }
static void balanced_enable(struct kunit *test)
{
	struct regulator *r = test->priv;
	KUNIT_EXPECT_EQ(test, regulator_is_enabled(r), 0);
	KUNIT_ASSERT_EQ(test, regulator_enable(r), 0);
	KUNIT_EXPECT_EQ(test, regulator_enable(r), 0);
	KUNIT_EXPECT_EQ(test, regulator_is_enabled(r), 1);
	KUNIT_EXPECT_EQ(test, regulator_disable(r), 0);
	KUNIT_EXPECT_EQ(test, regulator_is_enabled(r), 1);
	KUNIT_EXPECT_EQ(test, regulator_disable(r), 0);
	KUNIT_EXPECT_EQ(test, regulator_is_enabled(r), 0);
}
static void voltage_constraints(struct kunit *test)
{
	struct regulator *r = test->priv;
	KUNIT_EXPECT_EQ(test, regulator_set_voltage(r, 950000, 1000000), 0);
	KUNIT_EXPECT_EQ(test, regulator_get_voltage(r), 950000);
	KUNIT_EXPECT_EQ(test, regulator_set_voltage(r, 1300000, 1400000), -EINVAL);
	KUNIT_EXPECT_EQ(test, regulator_get_voltage(r), 950000);
	KUNIT_EXPECT_EQ(test, regulator_set_voltage(r, 900000, 900000), 0);
}
static void voltage_enumeration(struct kunit *test)
{
	struct regulator *r = test->priv;
	KUNIT_EXPECT_EQ(test, regulator_count_voltages(r), 2);
	KUNIT_EXPECT_EQ(test, regulator_list_voltage(r, 0), 900000);
	KUNIT_EXPECT_EQ(test, regulator_list_voltage(r, 1), 1200000);
	KUNIT_EXPECT_EQ(test, regulator_list_voltage(r, 2), -EINVAL);
}
static void mode_constraints(struct kunit *test)
{
	struct regulator *r = test->priv;
	KUNIT_EXPECT_EQ(test, regulator_set_mode(r, REGULATOR_MODE_NORMAL), 0);
	KUNIT_EXPECT_EQ(test, regulator_get_mode(r), (unsigned int)REGULATOR_MODE_NORMAL);
	KUNIT_EXPECT_EQ(test, regulator_set_mode(r, REGULATOR_MODE_IDLE), 0);
	KUNIT_EXPECT_EQ(test, regulator_get_mode(r), (unsigned int)REGULATOR_MODE_IDLE);
	KUNIT_EXPECT_EQ(test, regulator_set_mode(r, 0), -EINVAL);
}
static void load_requests(struct kunit *test)
{
	struct regulator *r = test->priv;
	KUNIT_ASSERT_EQ(test, regulator_enable(r), 0);
	KUNIT_EXPECT_EQ(test, regulator_set_load(r, 200), 0);
	KUNIT_EXPECT_EQ(test, regulator_get_mode(r), (unsigned int)REGULATOR_MODE_NORMAL);
	KUNIT_EXPECT_EQ(test, regulator_set_load(r, 0), 0);
	KUNIT_EXPECT_EQ(test, regulator_get_mode(r), (unsigned int)REGULATOR_MODE_IDLE);
	KUNIT_EXPECT_EQ(test, regulator_disable(r), 0);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(balanced_enable), KUNIT_CASE(voltage_constraints),
	KUNIT_CASE(voltage_enumeration), KUNIT_CASE(mode_constraints),
	KUNIT_CASE(load_requests), {}
};
static struct kunit_suite suite = {
	.name = "avr-regulators", .init = get_rail, .exit = put_rail, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
