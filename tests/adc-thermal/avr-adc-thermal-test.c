// SPDX-License-Identifier: GPL-2.0-only
/* Actual exported vendor conversions; no simulated PMIC or fabricated samples. */
#include <kunit/test.h>
#include <linux/iio/adc/qcom-vadc-common.h>
#include <linux/module.h>

static const struct adc5_data gen3 = {
	.full_scale_code_volt = 0x70e4, .full_scale_code_cur = 0x2ee0,
	.full_scale_code_raw = 0x4000,
};
static const struct adc5_data gen4 = {
	.full_scale_code_volt = 0x70e4, .full_scale_code_cur = 0x4e20,
	.full_scale_code_raw = 0xffff,
};
static void voltage_vectors(struct kunit *test)
{
	int result;
	KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_DEFAULT, 0, &gen3, 0, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 0);
	KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_DEFAULT, 0, &gen3, 0x70e4, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 1875000);
	KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_DEFAULT, 1, &gen4, 0x3872, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 2812500);
}
static void battery_resistance_vectors(struct kunit *test)
{
	int result;
	KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_PM5_GEN3_BATT_ID_100K, 0, &gen3, 8192, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 100000);
	KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_PM5_GEN4_BATT_ID_10K, 0, &gen4, 32768, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 10000);
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_PM5_GEN3_BATT_ID_100K, 0, &gen3, 16384, &result), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_PM5_GEN4_BATT_ID_10K, 0, &gen4, 65535, &result), -EINVAL);
}
static void temperature_units(struct kunit *test)
{
	int result;
	KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_THERM_100K_PU_PM7, 0, &gen3, 8192, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 25600); /* Pinned PM7 table: 100 kohm -> 25.6 C, in mC. */
	KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_PM5_GEN3_BATT_THERM_100K, 0, &gen3, 8192, &result), 0);
	KUNIT_EXPECT_EQ(test, result, 250); /* power_supply ABI: tenths of a degree */
}
static void ntc_monotonicity(struct kunit *test)
{
	int result, last = INT_MAX;
	unsigned int code;
	const struct adc5_data *data;
	unsigned int generation;
	for (generation = 0; generation < 2; generation++) {
		data = generation ? &gen4 : &gen3;
		last = INT_MAX;
		for (code = 1; code < data->full_scale_code_raw; code += 127) {
			KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_THERM_100K_PU_PM7, 0, data, code, &result), 0);
			KUNIT_EXPECT_LE(test, result, last);
			last = result;
		}
	}
}
static void threshold_roundtrip(struct kunit *test)
{
	const struct adc5_data *data;
	struct adc_tm_config config;
	unsigned int generation;
	int low, high, temp;
	for (generation = 0; generation < 2; generation++) {
		data = generation ? &gen4 : &gen3;
		for (temp = -30000; temp <= 100000; temp += 5000) {
			config.high_thr_temp = temp + 5000;
			config.low_thr_temp = temp;
			adc_tm_scale_therm_voltage_100k_gen3(&config, data);
			KUNIT_ASSERT_LT(test, config.low_thr_voltage, config.high_thr_voltage);
			KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_THERM_100K_PU_PM7, 0, data, config.low_thr_voltage, &high), 0);
			KUNIT_ASSERT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_THERM_100K_PU_PM7, 0, data, config.high_thr_voltage, &low), 0);
			KUNIT_EXPECT_GE(test, high, config.high_thr_temp);
			KUNIT_EXPECT_LE(test, low, config.low_thr_temp);
			KUNIT_EXPECT_LE(test, high - config.high_thr_temp, 200);
			KUNIT_EXPECT_LE(test, config.low_thr_temp - low, 200);
		}
	}
}
static void invalid_scale_arguments(struct kunit *test)
{
	int result = 12345;
	const struct adc5_data bad = { .full_scale_code_volt = 0,
		.full_scale_code_cur = 0x2ee0, .full_scale_code_raw = 0x4000 };
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(-1, 0, &gen3, 1, &result), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_INVALID, 0, &gen3, 1, &result), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_DEFAULT, UINT_MAX, &gen3, 1, &result), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_DEFAULT, 0, NULL, 1, &result), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_DEFAULT, 0, &gen3, 1, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_hw_scale(SCALE_HW_CALIB_DEFAULT, 0, &bad, 1, &result), -EINVAL);
	KUNIT_EXPECT_EQ(test, result, 12345);
}
static void dt_parameter_validation(struct kunit *test)
{
	unsigned int decimation[] = {85, 340, 1360};
	KUNIT_EXPECT_EQ(test, qcom_adc5_prescaling_from_dt(1, 1), 0);
	KUNIT_EXPECT_EQ(test, qcom_adc5_prescaling_from_dt(1, 3), 1);
	KUNIT_EXPECT_EQ(test, qcom_adc5_prescaling_from_dt(1, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_decimation_from_dt(340, decimation), 1);
	KUNIT_EXPECT_EQ(test, qcom_adc5_decimation_from_dt(341, decimation), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_avg_samples_from_dt(0), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_avg_samples_from_dt(3), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_adc5_avg_samples_from_dt(16), 4);
	KUNIT_EXPECT_EQ(test, qcom_adc5_avg_samples_from_dt(32), -EINVAL);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(voltage_vectors), KUNIT_CASE(battery_resistance_vectors),
	KUNIT_CASE(temperature_units), KUNIT_CASE(ntc_monotonicity),
	KUNIT_CASE(threshold_roundtrip), KUNIT_CASE(invalid_scale_arguments),
	KUNIT_CASE(dt_parameter_validation), {}
};
static struct kunit_suite suite = { .name = "avr-adc-thermal", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
