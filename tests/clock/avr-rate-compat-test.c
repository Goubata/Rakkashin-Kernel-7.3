// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <kunit/clk.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/module.h>
#include "../../ports/qcom-clock-core/rate-compat.h"

struct bridge_fixture {
	struct clk_hw hw;
	struct clk *consumer;
	unsigned int divisor;
	bool read_only;
	bool fail_round;
	const struct clk_div_table *table;
	unsigned long flags;
	unsigned long last_parent;
};

KUNIT_DEFINE_ACTION_WRAPPER(unregister_fixed, clk_hw_unregister_fixed_rate,
			   struct clk_hw *);

static long legacy_round(struct clk_hw *hw, unsigned long rate,
			 unsigned long *parent)
{
	struct bridge_fixture *f = container_of(hw, struct bridge_fixture, hw);
	long result;

	if (f->fail_round)
		return -EIO;
	if (f->read_only)
		result = qcom_vendor_divider_ro_round_rate(hw, rate, parent,
				f->table, 2, f->flags, f->divisor - 1);
	else
		result = qcom_vendor_divider_round_rate(hw, rate, parent,
				f->table, 2, f->flags);
	f->last_parent = *parent;
	return result;
}

static int bridge_determine(struct clk_hw *hw, struct clk_rate_request *req)
{
	return qcom_vendor_determine_rate(hw, req, legacy_round);
}

static unsigned long bridge_recalc(struct clk_hw *hw, unsigned long parent)
{
	return parent / container_of(hw, struct bridge_fixture, hw)->divisor;
}

static int bridge_set(struct clk_hw *hw, unsigned long rate, unsigned long parent)
{
	struct bridge_fixture *f = container_of(hw, struct bridge_fixture, hw);
	unsigned long divisor;

	if (!rate || parent % rate)
		return -EINVAL;
	divisor = parent / rate;
	if (f->read_only || divisor < 1 || divisor > 4)
		return -EINVAL;
	f->divisor = divisor;
	return 0;
}

static const struct clk_ops bridge_ops = {
	.recalc_rate = bridge_recalc,
	.determine_rate = bridge_determine,
	.set_rate = bridge_set,
};

static int register_bridge(struct kunit *test, struct bridge_fixture *f,
			   const char *name, const char *parent, unsigned long flags)
{
	struct clk_init_data init = {
		.name = name,
		.ops = &bridge_ops,
		.parent_names = &parent,
		.num_parents = 1,
		.flags = flags | CLK_GET_RATE_NOCACHE,
	};
	int ret;

	f->divisor = 1;
	f->hw.init = &init;
	ret = clk_hw_register_kunit(test, NULL, &f->hw);
	if (ret)
		return ret;
	f->consumer = clk_hw_get_clk_kunit(test, &f->hw, NULL);
	return PTR_ERR_OR_ZERO(f->consumer);
}

static int bridge_init(struct kunit *test)
{
	struct bridge_fixture *f;
	struct clk_hw *parent;
	int ret;

	f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	parent = clk_hw_register_fixed_rate(NULL, "avr-bridge-parent", NULL,
					   0, 24000000);
	if (IS_ERR(parent))
		return PTR_ERR(parent);
	ret = kunit_add_action_or_reset(test, unregister_fixed, parent);
	if (ret)
		return ret;
	ret = register_bridge(test, f, "avr-bridge-divider", "avr-bridge-parent", 0);
	if (ret)
		return ret;
	test->priv = f;
	return 0;
}

static void exact_division_preserves_parent(struct kunit *test)
{
	struct bridge_fixture *f = test->priv;

	KUNIT_EXPECT_EQ(test, clk_round_rate(f->consumer, 8000000), 8000000L);
	KUNIT_EXPECT_EQ(test, f->last_parent, 24000000UL);
}

static void read_only_divider_uses_programmed_value(struct kunit *test)
{
	struct bridge_fixture *f = test->priv;

	f->read_only = true;
	f->divisor = 2;
	KUNIT_EXPECT_EQ(test, clk_round_rate(f->consumer, 9000000), 12000000L);
	KUNIT_EXPECT_EQ(test, f->last_parent, 24000000UL);
}

static void explicit_divider_table_is_respected(struct kunit *test)
{
	static const struct clk_div_table table[] = {
		{ .val = 0, .div = 1 }, { .val = 1, .div = 2 },
		{ .val = 3, .div = 4 }, { }
	};
	struct bridge_fixture *f = test->priv;

	f->table = table;
	f->flags = CLK_DIVIDER_POWER_OF_TWO;
	KUNIT_EXPECT_EQ(test, clk_round_rate(f->consumer, 12000000), 12000000L);
	KUNIT_EXPECT_EQ(test, f->last_parent, 24000000UL);
}

struct adjustable_parent { struct clk_hw hw; unsigned long rate; };
static unsigned long parent_recalc(struct clk_hw *hw, unsigned long unused)
{
	return container_of(hw, struct adjustable_parent, hw)->rate;
}
static int parent_determine(struct clk_hw *hw, struct clk_rate_request *req)
{
	return 0;
}
static int parent_set(struct clk_hw *hw, unsigned long rate, unsigned long unused)
{
	container_of(hw, struct adjustable_parent, hw)->rate = rate;
	return 0;
}
static const struct clk_ops adjustable_ops = {
	.recalc_rate = parent_recalc, .determine_rate = parent_determine,
	.set_rate = parent_set,
};

static void parent_rate_adjustment_is_applied(struct kunit *test)
{
	struct adjustable_parent *p;
	struct bridge_fixture *f;
	struct clk_init_data init = {
		.name = "avr-adjustable-parent", .ops = &adjustable_ops,
	};

	p = kunit_kzalloc(test, sizeof(*p), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, p);
	f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, f);
	p->rate = 24000000;
	p->hw.init = &init;
	KUNIT_ASSERT_EQ(test, clk_hw_register_kunit(test, NULL, &p->hw), 0);
	KUNIT_ASSERT_EQ(test, register_bridge(test, f, "avr-adjustable-divider",
				"avr-adjustable-parent", CLK_SET_RATE_PARENT), 0);
	KUNIT_ASSERT_EQ(test, clk_set_rate(f->consumer, 7000000), 0);
	KUNIT_EXPECT_EQ(test, clk_get_rate(f->consumer), 7000000UL);
	KUNIT_EXPECT_NE(test, p->rate, 24000000UL);
	KUNIT_EXPECT_EQ(test, p->rate, f->last_parent);
	KUNIT_EXPECT_EQ(test, p->rate % 7000000, 0UL);
	KUNIT_EXPECT_GE(test, p->rate / 7000000, 1UL);
	KUNIT_EXPECT_LE(test, p->rate / 7000000, 4UL);
}

static void backend_error_is_propagated(struct kunit *test)
{
	struct bridge_fixture *f = test->priv;

	f->fail_round = true;
	KUNIT_EXPECT_EQ(test, clk_round_rate(f->consumer, 12000000), (long)-EIO);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(exact_division_preserves_parent),
	KUNIT_CASE(read_only_divider_uses_programmed_value),
	KUNIT_CASE(explicit_divider_table_is_respected),
	KUNIT_CASE(parent_rate_adjustment_is_applied),
	KUNIT_CASE(backend_error_is_propagated),
	{}
};
static struct kunit_suite suite = {
	.name = "avr_clock_rate_compat", .init = bridge_init, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Rate-request migration checks for the AVR clock port");
