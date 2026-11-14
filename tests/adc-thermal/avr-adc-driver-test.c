// SPDX-License-Identifier: GPL-2.0-only
/* Exercise the actual ADC driver helpers without registering a PMIC provider. */
#include <kunit/test.h>
#include <linux/platform_device.h>
#include <linux/kthread.h>
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
	struct platform_device *provider, *consumer;
	bool provider_driver_registered, consumer_driver_registered;
	unsigned int remove_event, provider_removed_at, consumer_removed_at;
	struct adc5_chip *linked_chip;
	struct adc5_base_data base;
	u8 registers[256];
	unsigned int notifications;
	struct adc_tm_param request;
	int disable_ret;
	struct completion callback_entered, callback_release, disable_started, disable_done;
	bool callback_timed_out;
};
static int ram_read(void *ctx, unsigned int reg, unsigned int *value)
{
	struct fixture *f = ctx;
	if (reg >= sizeof(f->registers))
		return -ERANGE;
	*value = f->registers[reg];
	return 0;
}
static int ram_write(void *ctx, unsigned int reg, unsigned int value)
{
	struct fixture *f = ctx;
	if (reg >= sizeof(f->registers))
		return -ERANGE;
	/* Test transport completes requests immediately; no physical samples. */
	f->registers[reg] = reg == ADC5_GEN3_CONV_REQ ? 0 : value;
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
	f->dev = root_device_register("avr_adc_unit");
	if (IS_ERR(f->dev))
		return PTR_ERR(f->dev);
	f->chip.regmap = regmap_init(f->dev, &ram_bus, f, &ram_config);
	if (IS_ERR(f->chip.regmap)) {
		int ret = PTR_ERR(f->chip.regmap);
		root_device_unregister(f->dev);
		return ret;
	}
	f->chip.base = &f->base; f->chip.num_sdams = 1;
	f->registers[ADC5_GEN3_HS] = ADC5_GEN3_HS_READY;
	init_completion(&f->callback_entered); init_completion(&f->callback_release);
	init_completion(&f->disable_started); init_completion(&f->disable_done);
	f->chip.dev = f->dev; f->chip.nchannels = 1;
	f->chip.chan_props = &f->prop; f->prop.chip = &f->chip;
	f->prop.adc_tm = ADC_TM_NON_THERMAL;
	f->prop.adc_rscale_fn = SCALE_R_ABSOLUTE;
	f->prop.data = &adc5_gen3_data_pmic;
	INIT_WORK(&f->prop.work, notify_adc_tm_fn);
	INIT_LIST_HEAD(&f->prop.thr_list);
	INIT_LIST_HEAD(&f->chip.list);
	mutex_init(&f->chip.lock);
	INIT_WORK(&f->chip.tm_handler_work, tm_handler_work);
	INIT_WORK(&f->chip.tm_err_handler_work, tm_err_handler_work);
	test->priv = f;
	return 0;
}
static int provider_probe(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	platform_set_drvdata(pdev, &f->chip);
	return 0;
}
static void provider_remove(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	f->provider_removed_at = ++f->remove_event;
}
static int consumer_probe(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	f->linked_chip = adc_tm_link_provider(&pdev->dev, &f->provider->dev);
	return PTR_ERR_OR_ZERO(f->linked_chip);
}
static void consumer_remove(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	f->consumer_removed_at = ++f->remove_event;
}
static struct platform_driver provider_driver = {
	.driver.name = "avr-adc-provider-test", .probe = provider_probe, .remove = provider_remove,
};
static struct platform_driver consumer_driver = {
	.driver.name = "avr-adc-client-test", .probe = consumer_probe, .remove = consumer_remove,
};
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	complete_all(&f->callback_release);
	if (!IS_ERR_OR_NULL(f->consumer))
		platform_device_unregister(f->consumer);
	if (!IS_ERR_OR_NULL(f->provider))
		platform_device_unregister(f->provider);
	if (f->consumer_driver_registered)
		platform_driver_unregister(&consumer_driver);
	if (f->provider_driver_registered)
		platform_driver_unregister(&provider_driver);
	adc5_gen3_cleanup(&f->chip);
	regmap_exit(f->chip.regmap);
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
	KUNIT_EXPECT_PTR_NE(test, client->param, &param);
	KUNIT_EXPECT_EQ(test, client->param->low_thr, param.low_thr);
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
static void supplier_link_orders_removal(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_ASSERT_EQ(test, platform_driver_register(&provider_driver), 0);
	f->provider_driver_registered = true;
	KUNIT_ASSERT_EQ(test, platform_driver_register(&consumer_driver), 0);
	f->consumer_driver_registered = true;
	f->provider = platform_device_register_data(NULL, "avr-adc-provider-test",
		PLATFORM_DEVID_AUTO, &f, sizeof(f));
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->provider));
	KUNIT_ASSERT_TRUE(test, device_is_bound(&f->provider->dev));
	/* A busy supplier must defer rather than acquiring its lock recursively. */
	device_lock(&f->provider->dev);
	KUNIT_EXPECT_EQ(test, PTR_ERR(adc_tm_link_provider(f->dev, &f->provider->dev)), -EPROBE_DEFER);
	device_unlock(&f->provider->dev);
	f->consumer = platform_device_register_data(NULL, "avr-adc-client-test",
		PLATFORM_DEVID_AUTO, &f, sizeof(f));
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->consumer));
	KUNIT_ASSERT_TRUE(test, device_is_bound(&f->consumer->dev));
	KUNIT_EXPECT_PTR_EQ(test, f->linked_chip, &f->chip);
	device_release_driver(&f->provider->dev);
	KUNIT_EXPECT_FALSE(test, device_is_bound(&f->consumer->dev));
	KUNIT_EXPECT_EQ(test, f->consumer_removed_at, 1U);
	KUNIT_EXPECT_EQ(test, f->provider_removed_at, 2U);
	KUNIT_EXPECT_EQ(test, PTR_ERR(adc_tm_link_provider(f->dev, &f->provider->dev)), -EPROBE_DEFER);
}
static void self_disabling_notification(enum adc_tm_state state, void *context)
{
	struct fixture *f = context;
	f->notifications++;
	f->disable_ret = adc_tm_disable_chan_meas_gen3(&f->chip, &f->request);
}
static void callback_disables_pending_notification(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct adc_tm_client_info *client;
	f->request = (struct adc_tm_param) { .low_thr = 500000, .high_thr = 1000000,
		.state_request = ADC_TM_HIGH_LOW_THR_ENABLE,
		.threshold_notification = self_disabling_notification, .btm_ctx = f };
	KUNIT_ASSERT_EQ(test, adc_tm_add_to_list(&f->chip, 0, &f->request), 0);
	client = list_first_entry(&f->prop.thr_list, struct adc_tm_client_info, list);
	client->notify_low_thr = true; client->notify_high_thr = true;
	f->prop.req_wq = adc_tm_create_notify_queue();
	KUNIT_ASSERT_NOT_NULL(test, f->prop.req_wq);
	/* Execute actual worker, including configure, callback and self-disable. */
	queue_work(f->prop.req_wq, &f->prop.work);
	flush_work(&f->prop.work);
	KUNIT_EXPECT_EQ(test, f->disable_ret, 0);
	KUNIT_EXPECT_EQ(test, f->notifications, 1U);
	KUNIT_EXPECT_FALSE(test, client->notify_high_thr);
	KUNIT_EXPECT_PTR_EQ(test, client->param->btm_ctx, NULL);
}
static void caller_parameter_is_not_retained(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct adc_tm_param param = { .low_thr = 123, .high_thr = 456 };
	struct adc_tm_client_info *client;
	KUNIT_ASSERT_EQ(test, adc_tm_add_to_list(&f->chip, 0, &param), 0);
	client = list_first_entry(&f->prop.thr_list, struct adc_tm_client_info, list);
	param.low_thr = -987;
	KUNIT_EXPECT_EQ(test, client->param->low_thr, 123);
	KUNIT_EXPECT_EQ(test, client->low_thr_requested, 123);
}
static void blocking_notification(enum adc_tm_state state, void *context)
{
	struct fixture *f = context;
	complete(&f->callback_entered);
	f->callback_timed_out = !wait_for_completion_timeout(&f->callback_release, msecs_to_jiffies(2000));
}
static int disable_thread(void *context)
{
	struct fixture *f = context;
	complete(&f->disable_started);
	f->disable_ret = adc_tm_disable_chan_meas_gen3(&f->chip, &f->request);
	complete(&f->disable_done);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(HZ);
	return 0;
}
static void external_disable_waits_for_callback(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct adc_tm_client_info *client;
	struct task_struct *thread;
	unsigned long early;
	f->request = (struct adc_tm_param) { .low_thr = 500000, .high_thr = 1000000,
		.state_request = ADC_TM_HIGH_LOW_THR_ENABLE,
		.threshold_notification = blocking_notification, .btm_ctx = f };
	KUNIT_ASSERT_EQ(test, adc_tm_add_to_list(&f->chip, 0, &f->request), 0);
	client = list_first_entry(&f->prop.thr_list, struct adc_tm_client_info, list);
	client->notify_low_thr = true;
	f->prop.req_wq = adc_tm_create_notify_queue();
	KUNIT_ASSERT_NOT_NULL(test, f->prop.req_wq);
	queue_work(f->prop.req_wq, &f->prop.work);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->callback_entered, msecs_to_jiffies(1000)), 0UL);
	thread = kthread_run(disable_thread, f, "avr_adc_disable");
	if (IS_ERR(thread)) {
		complete(&f->callback_release);
		KUNIT_FAIL(test, "disable thread allocation failed");
		return;
	}
	wait_for_completion(&f->disable_started);
	early = wait_for_completion_timeout(&f->disable_done, msecs_to_jiffies(20));
	complete(&f->callback_release);
	if (!early)
		wait_for_completion(&f->disable_done);
	kthread_stop(thread);
	flush_work(&f->prop.work);
	KUNIT_EXPECT_EQ(test, early, 0UL);
	KUNIT_EXPECT_EQ(test, f->disable_ret, 0);
	KUNIT_EXPECT_FALSE(test, f->callback_timed_out);
	KUNIT_EXPECT_PTR_EQ(test, client->param->btm_ctx, NULL);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(client_registration), KUNIT_CASE(invalid_client_input),
	KUNIT_CASE(threshold_polarity), KUNIT_CASE(queue_cleanup),
	KUNIT_CASE(generation_parameters), KUNIT_CASE(supplier_link_orders_removal),
	KUNIT_CASE(callback_disables_pending_notification), KUNIT_CASE(caller_parameter_is_not_retained),
	KUNIT_CASE(external_disable_waits_for_callback), {}
};
static struct kunit_suite suite = { .name = "avr-adc-driver",
	.init = setup, .exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
