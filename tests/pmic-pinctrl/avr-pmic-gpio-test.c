// SPDX-License-Identifier: GPL-2.0-only
/* Compile the actual callback source against a RAM regmap. No PMIC driver
 * registration or hardware model is installed by this test module.
 */
#include <kunit/test.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(drv) static __maybe_unused const void * const avr_driver_reference = &(drv)
#undef MODULE_DEVICE_TABLE
#define MODULE_DEVICE_TABLE(type, name)
#include "../../ports/pmic-pinctrl/pinctrl-spmi-gpio.c"

struct fixture {
	struct pmic_gpio_state state;
	struct pinctrl_dev ctrl;
	struct pinctrl_desc desc;
	struct pinctrl_pin_desc pin;
	struct pmic_gpio_pad pad;
	struct device *dev;
	struct regmap *map;
	u8 bytes[256];
	int read_error, write_error;
	unsigned int writes;
};
static int ram_read(void *context, unsigned int reg, unsigned int *value)
{
	struct fixture *f = context;
	if (f->read_error)
		return f->read_error;
	if (reg >= sizeof(f->bytes))
		return -ERANGE;
	*value = f->bytes[reg];
	return 0;
}
static int ram_write(void *context, unsigned int reg, unsigned int value)
{
	struct fixture *f = context;
	if (f->write_error)
		return f->write_error;
	if (reg >= sizeof(f->bytes))
		return -ERANGE;
	f->bytes[reg] = value; f->writes++;
	return 0;
}
static const struct regmap_bus ram_bus = { .reg_read = ram_read, .reg_write = ram_write };
static const struct regmap_config ram_config = {
	.reg_bits = 16, .val_bits = 8, .max_register = 255, .cache_type = REGCACHE_NONE,
};
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	int ret;
	if (!f)
		return -ENOMEM;
	f->dev = root_device_register("avr_gpio_unit");
	if (IS_ERR(f->dev))
		return PTR_ERR(f->dev);
	f->map = regmap_init(f->dev, &ram_bus, f, &ram_config);
	if (IS_ERR(f->map)) {
		ret = PTR_ERR(f->map);
		goto remove_device;
	}
	f->state.dev = f->dev; f->state.map = f->map; f->state.ctrl = &f->ctrl;
	f->ctrl.dev = f->dev; f->ctrl.desc = &f->desc; f->ctrl.driver_data = &f->state;
	f->desc.pins = &f->pin; f->desc.npins = 1;
	f->pin.drv_data = &f->pad;
	f->pad.num_sources = 8; f->pad.is_enabled = true; f->pad.lv_mv_type = true;
	f->state.chip = pmic_gpio_gpio_template;
	f->state.chip.parent = f->dev; f->state.chip.base = -1;
	f->state.chip.ngpio = 1; f->state.chip.label = "avr_gpio_unit";
	f->state.chip.of_gpio_n_cells = 2; f->state.chip.can_sleep = true;
	ret = gpiochip_add_data(&f->state.chip, &f->state);
	if (ret)
		goto remove_map;
	test->priv = f;
	return 0;
remove_map:
	regmap_exit(f->map);
remove_device:
	root_device_unregister(f->dev);
	return ret;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	gpiochip_remove(&f->state.chip);
	regmap_exit(f->map);
	root_device_unregister(f->dev);
}
static void physical_pin_bounds(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct of_phandle_args spec = { .args_count = 2, .args = {1, 1} };
	u32 flags = 0;
	KUNIT_EXPECT_EQ(test, pmic_gpio_of_xlate(&f->state.chip, &spec, &flags), 0);
	KUNIT_EXPECT_EQ(test, flags, 1U);
	spec.args[0] = 0;
	KUNIT_EXPECT_EQ(test, pmic_gpio_of_xlate(&f->state.chip, &spec, &flags), -EINVAL);
	spec.args[0] = 2;
	KUNIT_EXPECT_EQ(test, pmic_gpio_of_xlate(&f->state.chip, &spec, &flags), -EINVAL);
	spec.args[0] = 1; spec.args_count = 1;
	KUNIT_EXPECT_EQ(test, pmic_gpio_of_xlate(&f->state.chip, &spec, &flags), -EINVAL);
}
static void set_and_get_level(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_EXPECT_EQ(test, pmic_gpio_set(&f->state.chip, 0, 1), 0);
	KUNIT_EXPECT_GT(test, f->writes, 0U);
	KUNIT_EXPECT_EQ(test, pmic_gpio_get(&f->state.chip, 0), 1);
	KUNIT_EXPECT_EQ(test, pmic_gpio_set(&f->state.chip, 0, 0), 0);
	KUNIT_EXPECT_EQ(test, pmic_gpio_get(&f->state.chip, 0), 0);
}
static void write_error_propagates(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->write_error = -ETIMEDOUT;
	KUNIT_EXPECT_EQ(test, pmic_gpio_set(&f->state.chip, 0, 1), -ETIMEDOUT);
}
static void input_error_propagates(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->pad.input_enabled = true; f->read_error = -EIO;
	KUNIT_EXPECT_EQ(test, pmic_gpio_get(&f->state.chip, 0), -EIO);
}
static void cmos_direction_transitions(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->pad.output_enabled = true;
	KUNIT_EXPECT_EQ(test, pmic_gpio_direction_input(&f->state.chip, 0), 0);
	KUNIT_EXPECT_FALSE(test, f->pad.output_enabled);
	KUNIT_EXPECT_TRUE(test, f->pad.input_enabled);
	KUNIT_EXPECT_EQ(test, pmic_gpio_direction_output(&f->state.chip, 0, 1), 0);
	KUNIT_EXPECT_TRUE(test, f->pad.output_enabled);
	KUNIT_EXPECT_FALSE(test, f->pad.input_enabled);
}
static void open_drain_samples_line(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->pad.buffer_type = PMIC_GPIO_OUT_BUF_OPEN_DRAIN_NMOS;
	KUNIT_EXPECT_EQ(test, pmic_gpio_direction_output(&f->state.chip, 0, 1), 0);
	KUNIT_EXPECT_TRUE(test, f->pad.input_enabled);
	f->bytes[PMIC_MPP_REG_RT_STS] = 0;
	KUNIT_EXPECT_EQ(test, pmic_gpio_get(&f->state.chip, 0), 0);
	KUNIT_EXPECT_EQ(test, f->pad.out_value, 1);
	f->bytes[PMIC_MPP_REG_RT_STS] = PMIC_MPP_REG_RT_STS_VAL_MASK;
	KUNIT_EXPECT_EQ(test, pmic_gpio_get(&f->state.chip, 0), 1);
}
static void invalid_source_is_rejected(struct kunit *test)
{
	struct fixture *f = test->priv;
	unsigned long config = pinconf_to_config_packed(PIN_CONFIG_POWER_SOURCE, 8);
	KUNIT_EXPECT_EQ(test, pmic_gpio_config_set(&f->ctrl, 0, &config, 1), -EINVAL);
	KUNIT_EXPECT_EQ(test, f->writes, 0U);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(physical_pin_bounds), KUNIT_CASE(set_and_get_level),
	KUNIT_CASE(write_error_propagates), KUNIT_CASE(input_error_propagates),
	KUNIT_CASE(cmos_direction_transitions), KUNIT_CASE(open_drain_samples_line),
	KUNIT_CASE(invalid_source_is_rejected), {}
};
static struct kunit_suite suite = {
	.name = "avr-pmic-gpio", .init = setup, .exit = cleanup, .test_cases = cases,
};
kunit_test_suite(suite);
