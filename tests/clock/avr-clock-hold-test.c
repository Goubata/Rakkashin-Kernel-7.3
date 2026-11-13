// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <kunit/clk.h>
#include <kunit/device.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/device.h>
#include <linux/device/driver.h>
#include <linux/module.h>

struct hold_clock {
 struct clk_hw hw;
 struct clk *clk;
 struct device *dev;
 bool enabled;
};
static int hold_enable(struct clk_hw *hw)
{
 container_of(hw, struct hold_clock, hw)->enabled = true;
 return 0;
}
static void hold_disable(struct clk_hw *hw)
{
 container_of(hw, struct hold_clock, hw)->enabled = false;
}
static int hold_is_enabled(struct clk_hw *hw)
{
 return container_of(hw, struct hold_clock, hw)->enabled;
}
static void fake_sync_state(struct device *dev) { }
static const struct clk_ops hold_ops = {
 .enable=hold_enable, .disable=hold_disable, .is_enabled=hold_is_enabled,
};
static struct hold_clock *make_clock(struct kunit *test, unsigned long flags)
{
 struct hold_clock *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
 struct clk_init_data init={ .name="avr-hold-test", .ops=&hold_ops, .flags=flags };
 int ret;
 if (!f) return ERR_PTR(-ENOMEM);
 f->dev=kunit_device_register(test,"avr-hold-device");
 if (IS_ERR(f->dev)) return ERR_CAST(f->dev);
 f->dev->driver->sync_state=fake_sync_state;
 f->enabled=true;
 f->hw.init=&init;
 ret=clk_hw_register_kunit(test,f->dev,&f->hw);
 if (ret) return ERR_PTR(ret);
 f->clk=clk_hw_get_clk_kunit(test,&f->hw,NULL);
 if (IS_ERR(f->clk)) return ERR_CAST(f->clk);
 return f;
}
static void boot_hold_releases_once(struct kunit *test)
{
 struct hold_clock *f=make_clock(test,0);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,f);
 KUNIT_EXPECT_TRUE(test,clk_hw_is_enabled(&f->hw));
 clk_sync_state(f->dev);
 KUNIT_EXPECT_FALSE(test,clk_hw_is_enabled(&f->hw));
 /* A repeated callback must neither underflow prepare_count nor disable twice. */
 clk_sync_state(f->dev);
 KUNIT_EXPECT_FALSE(test,clk_hw_is_enabled(&f->hw));
}
static void repeated_sync_preserves_consumer(struct kunit *test)
{
 struct hold_clock *f=make_clock(test,0);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,f);
 KUNIT_ASSERT_EQ(test,clk_prepare_enable(f->clk),0);
 clk_sync_state(f->dev);
 KUNIT_EXPECT_TRUE(test,clk_hw_is_enabled(&f->hw));
 clk_sync_state(f->dev);
 KUNIT_EXPECT_TRUE(test,clk_hw_is_enabled(&f->hw));
 clk_disable_unprepare(f->clk);
 KUNIT_EXPECT_FALSE(test,clk_hw_is_enabled(&f->hw));
}
static void opt_out_does_not_acquire_hold(struct kunit *test)
{
 struct hold_clock *f=make_clock(test,CLK_DONT_HOLD_STATE);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,f);
 /* Without a boot hold one balanced consumer cycle must disable the clock. */
 KUNIT_ASSERT_EQ(test,clk_prepare_enable(f->clk),0);
 clk_disable_unprepare(f->clk);
 KUNIT_EXPECT_FALSE(test,clk_hw_is_enabled(&f->hw));
 clk_sync_state(f->dev);
}
static struct kunit_case hold_cases[]={
 KUNIT_CASE(boot_hold_releases_once),
 KUNIT_CASE(repeated_sync_preserves_consumer),
 KUNIT_CASE(opt_out_does_not_acquire_hold),{}
};
static struct kunit_suite hold_suite={.name="avr_clock_boot_hold",.test_cases=hold_cases};
kunit_test_suite(hold_suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("AVR CCF boot-state retention regression tests");
