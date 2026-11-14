// SPDX-License-Identifier: GPL-2.0-only
/* Exercise the actual ADC driver helpers without registering a PMIC provider. */
#include <kunit/test.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(drv) static __maybe_unused const void * const avr_driver_reference = &(drv)
#undef MODULE_DEVICE_TABLE
#define MODULE_DEVICE_TABLE(type, name)
#include "../../ports/qcom-adc-thermal/qcom-spmi-adc5-gen3.c"

struct fixture {
	struct adc5_chip chip;
	struct adc5_channel_prop prop;
	struct device *dev;
	bool worker_ran;
};
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	f->dev = root_device_register("avr_adc_unit");
	if (IS_ERR(f->dev))
		return PTR_ERR(f->dev);
	f->chip.dev = f->dev; f->chip.nchannels = 1;
	f->chip.chan_props = &f->prop; f->prop.chip = &f->chip;
	f->prop.adc_tm = ADC_TM_NON_THERMAL;
	f->prop.adc_rscale_fn = SCALE_R_ABSOLUTE;
	INIT_LIST_HEAD(&f->prop.thr_list);
	INIT_LIST_HEAD(&f->chip.list);
	mutex_init(&f->chip.lock);
	INIT_WORK(&f->chip.tm_handler_work, tm_handler_work);
	INIT_WORK(&f->chip.tm_err_handler_work, tm_err_handler_work);
	test->priv = f;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	adc5_gen3_cleanup(&f->chip);
	root_device_unregister(f->dev);
}
static void client_registration(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct adc_tm_param param = { .low_thr = 500000, .high_thr = 1000000,
		.state_request = ADC_TM_HIGH_LOW_THR_ENABLE };
	struct adc_tm_client_info *client;
	KUNIT_ASSERT_EQ(test, adc_tm_add_to_list(&f->chip, 0, &param), 0);
	KUNIT_ASSERT_FALSE(test, list_empty(&f->prop.thr_list));
	client = list_first_entry(&f->prop.thr_list, struct adc_tm_client_info, list);
	KUNIT_EXPECT_PTR_EQ(test, client->param, &param);
	KUNIT_EXPECT_EQ(test, param.id, (unsigned long)client);
	param.low_thr = 600000;
	KUNIT_ASSERT_EQ(test, adc_tm_add_to_list(&f->chip, 0, &param), 0);
	KUNIT_EXPECT_TRUE(test, list_is_singular(&f->prop.thr_list));
	KUNIT_EXPECT_EQ(test, client->low_thr_requested, 600000);
}
static void invalid_client_input(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct adc_tm_param param = {};
	KUNIT_EXPECT_EQ(test, adc_tm_channel_measure_gen3(NULL, &param), -EINVAL);
	KUNIT_EXPECT_EQ(test, adc_tm_channel_measure_gen3(&f->chip, NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, adc_tm_disable_chan_meas_gen3(NULL, &param), -EINVAL);
	f->prop.adc_tm = ADC_TM_NONE;
	KUNIT_EXPECT_EQ(test, adc_tm_disable_chan_meas_gen3(&f->chip, &param), -EINVAL);
}
static void threshold_polarity(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct adc_tm_param param = { .low_thr = 500000, .high_thr = 1000000,
		.state_request = ADC_TM_HIGH_LOW_THR_ENABLE };
	KUNIT_ASSERT_EQ(test, adc_tm_add_to_list(&f->chip, 0, &param), 0);
	KUNIT_ASSERT_EQ(test, adc_tm_gen3_manage_thresholds(&f->prop), 0);
	KUNIT_EXPECT_EQ(test, f->prop.low_thr_en, 1);
	KUNIT_EXPECT_EQ(test, f->prop.high_thr_en, 1);
	KUNIT_EXPECT_GT(test, f->prop.high_thr_voltage, f->prop.low_thr_voltage);
	f->prop.adc_rscale_fn = SCALE_RSCALE_NONE;
	KUNIT_EXPECT_EQ(test, adc_tm_gen3_manage_thresholds(&f->prop), -EBADF);
}
static void cleanup_worker(struct work_struct *work)
{
	struct adc5_channel_prop *prop = container_of(work, struct adc5_channel_prop, work);
	struct fixture *f = container_of(prop, struct fixture, prop);
	mutex_lock(&f->chip.lock);
	f->worker_ran = true;
	mutex_unlock(&f->chip.lock);
}
static void queue_cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->prop.req_wq = adc_tm_create_notify_queue();
	KUNIT_ASSERT_NOT_NULL(test, f->prop.req_wq);
	INIT_WORK(&f->prop.work, cleanup_worker);
	queue_work(f->prop.req_wq, &f->prop.work);
	adc5_gen3_cleanup(&f->chip);
	KUNIT_EXPECT_PTR_EQ(test, f->prop.req_wq, NULL);
	KUNIT_EXPECT_TRUE(test, f->worker_ran);
	KUNIT_EXPECT_TRUE(test, list_empty(&f->chip.list));
	/* The test fixture exit repeats cleanup; no double-free or stale dentry. */
}
static void generation_parameters(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, adc5_gen3_data_pmic.full_scale_code_raw, 0x4000U);
	KUNIT_EXPECT_EQ(test, adc5_gen4_data_pmic.full_scale_code_raw, 0xffffU);
	KUNIT_EXPECT_EQ(test, adc5_gen3_data_pmic.full_scale_code_cur, 0x2ee0U);
	KUNIT_EXPECT_EQ(test, adc5_gen4_data_pmic.full_scale_code_cur, 0x4e20U);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(client_registration), KUNIT_CASE(invalid_client_input),
	KUNIT_CASE(threshold_polarity), KUNIT_CASE(queue_cleanup),
	KUNIT_CASE(generation_parameters), {}
};
static struct kunit_suite suite = { .name = "avr-adc-driver",
	.init = setup, .exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
