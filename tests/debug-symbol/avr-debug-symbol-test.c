// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/kallsyms.h>
#include <linux/module.h>
#include <linux/sched/task.h>

static void table_available(struct kunit *test)
{
	KUNIT_EXPECT_TRUE(test, debug_symbol_available());
}
static void real_data_symbol(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, debug_symbol_lookup_name("init_task"),
			(unsigned long)&init_task);
}
static void real_function_symbol(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, debug_symbol_lookup_name("debug_symbol_lookup_name"),
			(unsigned long)debug_symbol_lookup_name);
}
static void real_module_symbol(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test,
		debug_symbol_lookup_name("avr_debug_symbol_test:real_module_symbol"),
		(unsigned long)real_module_symbol);
}
static void missing_and_invalid_names(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, debug_symbol_lookup_name(NULL), 0UL);
	KUNIT_EXPECT_EQ(test, debug_symbol_lookup_name(""), 0UL);
	KUNIT_EXPECT_EQ(test, debug_symbol_lookup_name("avr_no_such_symbol_7_3_19d8e2"), 0UL);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(table_available), KUNIT_CASE(real_data_symbol),
	KUNIT_CASE(real_function_symbol), KUNIT_CASE(real_module_symbol),
	KUNIT_CASE(missing_and_invalid_names), {}
};
static struct kunit_suite suite = { .name = "avr-debug-symbol", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Actual native diagnostic symbol lookup tests");
