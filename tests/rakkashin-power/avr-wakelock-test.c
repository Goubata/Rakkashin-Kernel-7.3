// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/device.h>
#include <linux/pm_wakeup.h>
static void release_source(void *data) {wakeup_source_unregister(data);}
static void expect_source(struct kunit *test,const char *name,bool blocked)
{
 struct wakeup_source *ws=wakeup_source_register(NULL,name);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,ws);
 KUNIT_ASSERT_EQ(test,kunit_add_action_or_reset(test,release_source,ws),0);
 __pm_stay_awake(ws);
 KUNIT_EXPECT_EQ(test,(bool)ws->active,!blocked);
 KUNIT_EXPECT_EQ(test,ws->event_count,blocked ? 0UL : 1UL);
 __pm_relax(ws);
 KUNIT_EXPECT_FALSE(test,ws->active);
}
static void user_filter(struct kunit *t) {expect_source(t,"avr-test-blocked",true);}
static void default_filter(struct kunit *t) {expect_source(t,"qcom_rx_wakelock",true);}
static void normal_source(struct kunit *t) {expect_source(t,"avr-test-normal",false);}
static void exact_match(struct kunit *t) {expect_source(t,"avr-test-blocked-extra",false);}
static void long_name(struct kunit *t) {expect_source(t,"avr-test-blocked-abcdefghijklmnopqrstuvwxyz-abcdefghijklmnopqrstuvwxyz",false);}
static struct kunit_case cases[]={KUNIT_CASE(user_filter),KUNIT_CASE(default_filter),
 KUNIT_CASE(normal_source),KUNIT_CASE(exact_match),KUNIT_CASE(long_name),{}};
static struct kunit_suite suite={.name="avr-wakelock",.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");

MODULE_DESCRIPTION("Rakkashin wakeup-source filter runtime tests");
