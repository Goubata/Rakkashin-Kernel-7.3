// SPDX-License-Identifier: GPL-2.0-only
/* Actual thermal alarm source, native thermal trips and a RAM-only regmap. */
#include <kunit/test.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(drv) static __maybe_unused const void * const avr_driver_reference = &(drv)
#undef MODULE_DEVICE_TABLE
#define MODULE_DEVICE_TABLE(type, name)
#include "../../ports/qcom-adc-thermal/qcom-spmi-temp-alarm.c"

struct fixture {
	struct qpnp_tm_chip chip;
	u8 registers[256];
	int read_error, write_error;
	int fail_register;
};
static int ram_read(void *ctx, unsigned int reg, unsigned int *value)
{
	struct fixture *f = ctx;
	if (f->read_error)
		return f->read_error;
	if (reg >= sizeof(f->registers))
		return -ERANGE;
	*value = f->registers[reg];
	return 0;
}
static int ram_write(void *ctx, unsigned int reg, unsigned int value)
{
	struct fixture *f = ctx;
	if (f->write_error && (f->fail_register < 0 || reg == f->fail_register))
		return f->write_error;
	if (reg >= sizeof(f->registers))
		return -ERANGE;
	f->registers[reg] = value;
	return 0;
}
static const struct regmap_bus ram_bus = { .reg_read = ram_read, .reg_write = ram_write };
static const struct regmap_config ram_config = { .reg_bits = 16, .val_bits = 8,
	.max_register = 255, .cache_type = REGCACHE_NONE };
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	f->chip.dev = root_device_register("avr_temp_alarm_unit");
	if (IS_ERR(f->chip.dev))
		return PTR_ERR(f->chip.dev);
	f->chip.map = regmap_init(f->chip.dev, &ram_bus, f, &ram_config);
	if (IS_ERR(f->chip.map)) {
		int ret = PTR_ERR(f->chip.map);
		root_device_unregister(f->chip.dev);
		return ret;
	}
	f->fail_register = -1;
	f->chip.subtype = QPNP_TM_SUBTYPE_GEN1;
	f->chip.temp_map = &temp_map_gen1;
	mutex_init(&f->chip.lock);
	test->priv = f;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	if (!IS_ERR_OR_NULL(f->chip.tz_dev))
		thermal_zone_device_unregister(f->chip.tz_dev);
	regmap_exit(f->chip.map);
	root_device_unregister(f->chip.dev);
}
static void critical_write_failure(struct kunit *test)
{
	struct fixture *f = test->priv;
	int ret;
	f->chip.thresh = 2;
	f->write_error = -EIO;
	mutex_lock(&f->chip.lock);
	ret = qpnp_tm_update_critical_trip_temp(&f->chip, 125000);
	mutex_unlock(&f->chip.lock);
	KUNIT_EXPECT_EQ(test, ret, -EIO);
	KUNIT_EXPECT_EQ(test, f->chip.thresh, 2U);
}
static void revision_shutdown_guard(struct kunit *test)
{
	struct fixture *f = test->priv;
	unsigned int revision;
	int ret;
	f->chip.subtype = QPNP_TM_SUBTYPE_GEN2;
	f->chip.temp_map = &temp_map_gen2_v1;
	mutex_lock(&f->chip.lock);
	for (revision = 0x100; revision <= 0x102; revision++) {
		f->chip.dig_revision = revision;
		ret = qpnp_tm_update_critical_trip_temp(&f->chip, 120000);
		KUNIT_EXPECT_EQ(test, ret, 0);
		KUNIT_EXPECT_EQ(test, !!(f->registers[QPNP_TM_REG_SHUTDOWN_CTRL1] & SHUTDOWN_CTRL1_OVERRIDE_S2), revision == 0x102);
	}
	mutex_unlock(&f->chip.lock);
}
static void dac_limits_and_failed_write(struct kunit *test)
{
	struct fixture *f = test->priv;
	int ret;
	mutex_lock(&f->chip.lock);
	ret = qpnp_tm_set_temp_dac_thresh(&f->chip, 0, 119375);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, f->registers[QPNP_TM_REG_TEMP_DAC_STG1], (u8)63);
	KUNIT_EXPECT_EQ(test, qpnp_tm_set_temp_dac_thresh(&f->chip, 0, 119376), -EINVAL);
	KUNIT_EXPECT_EQ(test, qpnp_tm_set_temp_dac_thresh(&f->chip, 3, 100000), -EINVAL);
	f->write_error = -EIO;
	KUNIT_EXPECT_EQ(test, qpnp_tm_set_temp_dac_thresh(&f->chip, 0, 100000), -EIO);
	KUNIT_EXPECT_EQ(test, f->chip.temp_dac_map[0], 119375L);
	mutex_unlock(&f->chip.lock);
}
static void lite_register_preservation(struct kunit *test)
{
	struct fixture *f = test->priv;
	int ret;
	f->registers[QPNP_TM_REG_LITE_TEMP_CFG1] = 0xa3;
	mutex_lock(&f->chip.lock);
	ret = qpnp_tm_set_temp_lite_thresh(&f->chip, 0, 130000);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, f->registers[QPNP_TM_REG_LITE_TEMP_CFG1], (u8)0xa7);
	KUNIT_EXPECT_EQ(test, f->chip.temp_dac_map[0], 125000L);
	f->write_error = -EIO;
	KUNIT_EXPECT_EQ(test, qpnp_tm_set_temp_lite_thresh(&f->chip, 0, 145000), -EIO);
	KUNIT_EXPECT_EQ(test, f->chip.temp_dac_map[0], 125000L);
	mutex_unlock(&f->chip.lock);
}
static void stage_hysteresis_and_read_error(struct kunit *test)
{
	struct fixture *f = test->priv;
	int ret;
	f->registers[QPNP_TM_REG_STATUS] = 1;
	mutex_lock(&f->chip.lock);
	ret = qpnp_tm_update_temp_no_adc(&f->chip);
	KUNIT_EXPECT_EQ(test, ret, 0);
	KUNIT_EXPECT_EQ(test, f->chip.temp, 107000L);
	f->registers[QPNP_TM_REG_STATUS] = 0;
	KUNIT_EXPECT_EQ(test, qpnp_tm_update_temp_no_adc(&f->chip), 0);
	KUNIT_EXPECT_EQ(test, f->chip.temp, 103000L);
	f->read_error = -EIO;
	KUNIT_EXPECT_EQ(test, qpnp_tm_update_temp_no_adc(&f->chip), -EIO);
	KUNIT_EXPECT_EQ(test, f->chip.temp, 103000L);
	mutex_unlock(&f->chip.lock);
}
static const struct thermal_zone_device_ops test_ops = { .get_temp = qpnp_tm_get_temp };
static void attach_trips(struct kunit *test, bool lite)
{
	struct fixture *f = test->priv;
	struct thermal_trip trips[] = {
		{ .type = THERMAL_TRIP_HOT, .temperature = lite ? 125000 : 100000 },
		{ .type = THERMAL_TRIP_CRITICAL, .temperature = lite ? 130000 : 120000 },
		{ .type = THERMAL_TRIP_CRITICAL, .temperature = 145000 },
	};
	f->chip.tz_dev = thermal_zone_device_register_with_trips("avr_alarm_test", trips, 3,
		&f->chip, &test_ops, NULL, 0, 0);
	if (IS_ERR(f->chip.tz_dev)) {
		f->chip.tz_dev = NULL;
		KUNIT_FAIL(test, "native thermal zone registration failed");
	}
}
static void repeated_dac_configuration(struct kunit *test)
{
	struct fixture *f = test->priv;
	attach_trips(test, false);
	KUNIT_ASSERT_NOT_NULL(test, f->chip.tz_dev);
	KUNIT_ASSERT_EQ(test, qpnp_tm_temp_dac_update_trip_temps(&f->chip), 0);
	KUNIT_EXPECT_EQ(test, qpnp_tm_temp_dac_update_trip_temps(&f->chip), 0);
	KUNIT_EXPECT_EQ(test, f->chip.ntrips, 3U);
}
static void repeated_lite_configuration(struct kunit *test)
{
	struct fixture *f = test->priv;
	attach_trips(test, true);
	KUNIT_ASSERT_NOT_NULL(test, f->chip.tz_dev);
	KUNIT_ASSERT_EQ(test, qpnp_tm_temp_lite_update_trip_temps(&f->chip), 0);
	KUNIT_EXPECT_EQ(test, qpnp_tm_temp_lite_update_trip_temps(&f->chip), 0);
	KUNIT_EXPECT_EQ(test, f->chip.ntrips, 3U);
}
static void failed_alarm_enable(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct thermal_trip trip = { .type = THERMAL_TRIP_CRITICAL, .temperature = 130000 };
	f->chip.tz_dev = thermal_zone_device_register_with_trips("avr_alarm_fail_test", &trip, 1,
		&f->chip, &test_ops, NULL, 0, 0);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->chip.tz_dev));
	f->write_error = -EIO;
	f->fail_register = QPNP_TM_REG_ALARM_CTRL;
	KUNIT_EXPECT_EQ(test, qpnp_tm_init(&f->chip), -EIO);
	KUNIT_EXPECT_FALSE(test, f->chip.initialized);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(critical_write_failure), KUNIT_CASE(revision_shutdown_guard),
	KUNIT_CASE(dac_limits_and_failed_write), KUNIT_CASE(lite_register_preservation),
	KUNIT_CASE(stage_hysteresis_and_read_error), KUNIT_CASE(repeated_dac_configuration),
	KUNIT_CASE(repeated_lite_configuration), KUNIT_CASE(failed_alarm_enable), {}
};
static struct kunit_suite suite = { .name = "avr-temp-alarm", .init = setup,
	.exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
