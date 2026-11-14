// SPDX-License-Identifier: GPL-2.0-only
/* Actual vendor client/RX code, private software fixture; no firmware provider. */
#include <kunit/test.h>
#include <linux/kthread.h>
#include <linux/module.h>
#pragma push_macro("pr_fmt")
#undef pr_fmt
#pragma push_macro("module_init")
#pragma push_macro("module_exit")
#undef module_init
#undef module_exit
#define module_init(fn) static __used typeof(fn) *avr_init_reference __initdata = (fn)
#define module_exit(fn) static __used typeof(fn) *avr_exit_reference __exitdata = (fn)
#include "../../ports/qcom-pmic-glink/qti_pmic_glink.c"
#pragma pop_macro("module_exit")
#pragma pop_macro("module_init")
#pragma pop_macro("pr_fmt")

struct fixture {
	struct pmic_glink_dev provider;
	struct platform_device *child;
	struct pmic_glink_client *client;
	struct device *parent;
	struct rpmsg_device transport;
	struct platform_device *bound_provider, *bound_consumer;
	struct pmic_glink_dev *linked_provider;
	bool provider_driver_registered, consumer_driver_registered, provider_stopped;
	unsigned int remove_event, provider_removed_at, consumer_removed_at;
	struct completion entered, release, unregister_started, unregister_done;
	unsigned int messages, states;
	bool block_rx, self_unregister, callback_timeout;
	int unregister_ret, self_ret;
};
static int receive(void *private, void *data, size_t len)
{
	struct fixture *f = private;
	f->messages++;
	if (f->self_unregister)
		f->self_ret = pmic_glink_unregister_client(f->client);
	if (f->block_rx) {
		complete(&f->entered);
		f->callback_timeout = !wait_for_completion_timeout(&f->release, msecs_to_jiffies(2000));
	}
	return 0;
}
static void state_changed(void *private, enum pmic_glink_state state)
{
	struct fixture *f = private;
	f->states++;
	if (f->self_unregister)
		f->self_ret = pmic_glink_unregister_client(f->client);
}
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	f->parent = root_device_register("avr_glink_unit");
	if (IS_ERR(f->parent))
		return PTR_ERR(f->parent);
	f->child = platform_device_register_data(f->parent, "avr-glink-client-unit", PLATFORM_DEVID_AUTO, NULL, 0);
	if (IS_ERR(f->child)) {
		int ret = PTR_ERR(f->child);
		root_device_unregister(f->parent);
		return ret;
	}
	f->provider.dev = f->parent;
	f->provider.channel_name = "AVR_SOFTWARE_TEST_CHANNEL";
	mutex_init(&f->provider.client_lock);
	init_rwsem(&f->provider.rpdev_sem);
	spin_lock_init(&f->provider.rx_lock);
	idr_init(&f->provider.client_idr);
	INIT_LIST_HEAD(&f->provider.client_dev_list);
	INIT_LIST_HEAD(&f->provider.rx_list);
	INIT_LIST_HEAD(&f->provider.dev_list);
	INIT_WORK(&f->provider.rx_work, pmic_glink_rx_work);
	INIT_WORK(&f->provider.init_work, pmic_glink_init_work);
	f->provider.rx_wq = alloc_ordered_workqueue("avr_glink_rx_test", 0);
	if (!f->provider.rx_wq) {
		platform_device_unregister(f->child);
		root_device_unregister(f->parent);
		return -ENOMEM;
	}
	atomic_set(&f->provider.state, 1);
	init_completion(&f->entered); init_completion(&f->release);
	init_completion(&f->unregister_started); init_completion(&f->unregister_done);
	dev_set_drvdata(&f->transport.dev, &f->provider);
	pmic_glink_dev_add(&f->provider);
	test->priv = f;
	return 0;
}
/* Real driver core binding around a software-only provider fixture. Provider
 * MMIO/firmware probe is not emulated; its production remove path is exercised.
 */
static int provider_probe(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	mutex_lock(&pmic_glink_dev_lock);
	f->provider.dev = &pdev->dev;
	mutex_unlock(&pmic_glink_dev_lock);
	platform_set_drvdata(pdev, &f->provider);
	return 0;
}
static void provider_remove(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	f->provider_removed_at = ++f->remove_event;
	pmic_glink_remove(pdev);
	f->provider_stopped = true;
}
static int consumer_probe(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	f->linked_provider = pmic_glink_link_provider(&pdev->dev,
		&f->bound_provider->dev, f->provider.channel_name);
	return PTR_ERR_OR_ZERO(f->linked_provider);
}
static void consumer_remove(struct platform_device *pdev)
{
	struct fixture *f = *(struct fixture **)dev_get_platdata(&pdev->dev);
	f->consumer_removed_at = ++f->remove_event;
	pmic_glink_rpmsg_remove(&f->transport);
}
static struct platform_driver provider_driver = {
	.driver.name = "avr-glink-provider-unit", .probe = provider_probe, .remove = provider_remove,
};
static struct platform_driver consumer_driver = {
	.driver.name = "avr-glink-link-unit", .probe = consumer_probe, .remove = consumer_remove,
};
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	complete_all(&f->release);
	if (!IS_ERR_OR_NULL(f->bound_consumer))
		platform_device_unregister(f->bound_consumer);
	if (!IS_ERR_OR_NULL(f->bound_provider))
		platform_device_unregister(f->bound_provider);
	if (f->consumer_driver_registered)
		platform_driver_unregister(&consumer_driver);
	if (f->provider_driver_registered)
		platform_driver_unregister(&provider_driver);
	if (!f->provider_stopped) {
		flush_workqueue(f->provider.rx_wq);
		if (!IS_ERR_OR_NULL(f->client))
			pmic_glink_unregister_client(f->client);
		pmic_glink_dev_remove(&f->provider);
		destroy_workqueue(f->provider.rx_wq);
		idr_destroy(&f->provider.client_idr);
	}
	platform_device_unregister(f->child);
	root_device_unregister(f->parent);
}
static void register_client(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct pmic_glink_client_data config = { .name = "avr-test", .id = 32778,
		.msg_cb = receive, .state_cb = state_changed, .priv = f };
	f->client = pmic_glink_register_client(&f->child->dev, &config);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->client));
}
static int enqueue(struct fixture *f, u32 owner)
{
	struct rpmsg_device rpdev = {};
	struct pmic_glink_hdr header = { .owner = owner, .type = 1, .opcode = 0x1234 };
	dev_set_drvdata(&rpdev.dev, &f->provider);
	return pmic_glink_rpmsg_callback(&rpdev, &header, sizeof(header), NULL, 0);
}
static void registration_and_framing(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct pmic_glink_client_data config = { .name = "invalid", .id = 1, .msg_cb = receive };
	struct rpmsg_device rpdev = {};
	u8 short_frame[sizeof(struct pmic_glink_hdr)] = {};
	KUNIT_EXPECT_EQ(test, PTR_ERR(pmic_glink_register_client(&f->child->dev, NULL)), -EINVAL);
	config.id = U32_MAX;
	KUNIT_EXPECT_EQ(test, PTR_ERR(pmic_glink_register_client(&f->child->dev, &config)), -EINVAL);
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	dev_set_drvdata(&rpdev.dev, &f->provider);
	KUNIT_EXPECT_EQ(test, pmic_glink_rpmsg_callback(&rpdev, short_frame, -1, NULL, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, pmic_glink_rpmsg_callback(&rpdev, short_frame, sizeof(short_frame)-1, NULL, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, pmic_glink_write(f->client, short_frame, sizeof(short_frame)-1), -EINVAL);
	KUNIT_EXPECT_EQ(test, pmic_glink_write(f->client, short_frame, sizeof(short_frame)), -ENOTCONN);
}
static void dispatch_and_state(struct kunit *test)
{
	struct fixture *f = test->priv;
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	KUNIT_ASSERT_EQ(test, enqueue(f, f->client->id), 0);
	flush_workqueue(f->provider.rx_wq);
	KUNIT_EXPECT_EQ(test, f->messages, 1U);
	KUNIT_ASSERT_EQ(test, enqueue(f, 999), 0);
	flush_workqueue(f->provider.rx_wq);
	KUNIT_EXPECT_EQ(test, f->messages, 1U);
	pmic_glink_notify_clients(&f->provider, PMIC_GLINK_STATE_DOWN);
	KUNIT_EXPECT_EQ(test, f->states, 1U);
}
static void duplicate_owner_rejected(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct pmic_glink_client_data config = { .name = "duplicate", .id = 32778, .msg_cb = receive };
	struct pmic_glink_client *other;
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	other = pmic_glink_register_client(&f->child->dev, &config);
	KUNIT_EXPECT_TRUE(test, IS_ERR(other));
	if (!IS_ERR(other))
		pmic_glink_unregister_client(other);
}
static void self_unregister_rx_rejected(struct kunit *test)
{
	struct fixture *f = test->priv;
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	f->self_unregister = true;
	KUNIT_ASSERT_EQ(test, enqueue(f, f->client->id), 0);
	flush_workqueue(f->provider.rx_wq);
	KUNIT_EXPECT_EQ(test, f->self_ret, -EDEADLK);
}
static void self_unregister_state_rejected(struct kunit *test)
{
	struct fixture *f = test->priv;
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	f->self_unregister = true;
	pmic_glink_notify_clients(&f->provider, PMIC_GLINK_STATE_DOWN);
	KUNIT_EXPECT_EQ(test, f->self_ret, -EDEADLK);
}
static int unregister_thread(void *private)
{
	struct fixture *f = private;
	complete(&f->unregister_started);
	f->unregister_ret = pmic_glink_unregister_client(f->client);
	f->client = NULL;
	complete(&f->unregister_done);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(HZ);
	return 0;
}
static void unregister_waits_for_rx(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct task_struct *thread;
	unsigned long early;
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	f->block_rx = true;
	KUNIT_ASSERT_EQ(test, enqueue(f, f->client->id), 0);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->entered, msecs_to_jiffies(1000)), 0UL);
	thread = kthread_run(unregister_thread, f, "avr_glink_unregister");
	if (IS_ERR(thread)) {
		complete(&f->release);
		KUNIT_FAIL(test, "unregister thread allocation failed");
		return;
	}
	wait_for_completion(&f->unregister_started);
	early = wait_for_completion_timeout(&f->unregister_done, msecs_to_jiffies(20));
	complete(&f->release);
	if (!early)
		wait_for_completion(&f->unregister_done);
	kthread_stop(thread);
	flush_workqueue(f->provider.rx_wq);
	KUNIT_EXPECT_EQ(test, early, 0UL);
	KUNIT_EXPECT_EQ(test, f->unregister_ret, 0);
	KUNIT_EXPECT_FALSE(test, f->callback_timeout);
}
static void detached_transport_rejects_rx(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct pmic_glink_hdr header = { .owner = 32778, .type = 1, .opcode = 1 };
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	KUNIT_ASSERT_EQ(test, pmic_glink_rpmsg_callback(&f->transport, &header, sizeof(header), NULL, 0), 0);
	pmic_glink_rpmsg_remove(&f->transport);
	KUNIT_EXPECT_EQ(test, f->messages, 1U);
	KUNIT_EXPECT_PTR_EQ(test, dev_get_drvdata(&f->transport.dev), NULL);
	KUNIT_EXPECT_EQ(test, atomic_read(&f->provider.state), 0);
	KUNIT_EXPECT_TRUE(test, list_empty(&f->provider.rx_list));
	KUNIT_EXPECT_EQ(test, pmic_glink_rpmsg_callback(&f->transport, &header, sizeof(header), NULL, 0), -ENOTCONN);
	/* Repeating an already detached remove must not touch provider resources. */
	pmic_glink_rpmsg_remove(&f->transport);
}
static int detach_thread(void *private)
{
	struct fixture *f = private;
	complete(&f->unregister_started);
	pmic_glink_rpmsg_remove(&f->transport);
	complete(&f->unregister_done);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(HZ);
	return 0;
}
static void detach_waits_for_ingress_and_rx(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct pmic_glink_hdr header = { .owner = 32778, .type = 1, .opcode = 1 };
	struct task_struct *thread;
	unsigned long early;
	int index;
	register_client(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->client));
	f->block_rx = true;
	KUNIT_ASSERT_EQ(test, pmic_glink_rpmsg_callback(&f->transport, &header, sizeof(header), NULL, 0), 0);
	KUNIT_ASSERT_NE(test, wait_for_completion_timeout(&f->entered, msecs_to_jiffies(1000)), 0UL);
	/* Hold an ingress epoch to represent a callback that already saw pgdev. */
	index = srcu_read_lock(&pmic_glink_ingress);
	thread = kthread_run(detach_thread, f, "avr_glink_detach");
	if (IS_ERR(thread)) {
		srcu_read_unlock(&pmic_glink_ingress, index);
		complete(&f->release);
		KUNIT_FAIL(test, "detach thread allocation failed");
		return;
	}
	wait_for_completion(&f->unregister_started);
	early = wait_for_completion_timeout(&f->unregister_done, msecs_to_jiffies(20));
	KUNIT_EXPECT_EQ(test, early, 0UL);
	srcu_read_unlock(&pmic_glink_ingress, index);
	/* Even with ingress unblocked, the actual RX callback still holds work. */
	if (!early)
		early = wait_for_completion_timeout(&f->unregister_done, msecs_to_jiffies(20));
	complete(&f->release);
	if (!early)
		wait_for_completion(&f->unregister_done);
	kthread_stop(thread);
	KUNIT_EXPECT_EQ(test, early, 0UL);
	KUNIT_EXPECT_FALSE(test, f->callback_timeout);
	KUNIT_EXPECT_PTR_EQ(test, dev_get_drvdata(&f->transport.dev), NULL);
	KUNIT_EXPECT_TRUE(test, list_empty(&f->provider.rx_list));
}
static void detach_waits_for_ingress_without_work(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct task_struct *thread;
	unsigned long early, deadline;
	int index = srcu_read_lock(&pmic_glink_ingress);

	thread = kthread_run(detach_thread, f, "avr_glink_ingress");
	if (IS_ERR(thread)) {
		srcu_read_unlock(&pmic_glink_ingress, index);
		KUNIT_FAIL(test, "detach thread allocation failed");
		return;
	}
	wait_for_completion(&f->unregister_started);
	deadline = jiffies + msecs_to_jiffies(1000);
	while (READ_ONCE(f->transport.dev.driver_data) && time_before(jiffies, deadline))
		usleep_range(100, 200);
	KUNIT_EXPECT_PTR_EQ(test, READ_ONCE(f->transport.dev.driver_data), NULL);
	/* There is no RX work to block removal: only the held ingress epoch. */
	early = wait_for_completion_timeout(&f->unregister_done, msecs_to_jiffies(20));
	srcu_read_unlock(&pmic_glink_ingress, index);
	if (!early)
		wait_for_completion(&f->unregister_done);
	kthread_stop(thread);
	KUNIT_EXPECT_EQ(test, early, 0UL);
}
static void linked_provider_unbind_orders_transport_first(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct device *supplier;
	KUNIT_ASSERT_EQ(test, platform_driver_register(&provider_driver), 0);
	f->provider_driver_registered = true;
	KUNIT_ASSERT_EQ(test, platform_driver_register(&consumer_driver), 0);
	f->consumer_driver_registered = true;
	f->bound_provider = platform_device_register_data(NULL, "avr-glink-provider-unit",
		PLATFORM_DEVID_AUTO, &f, sizeof(f));
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->bound_provider));
	KUNIT_ASSERT_TRUE(test, device_is_bound(&f->bound_provider->dev));
	supplier = find_pmic_glink_supplier(f->provider.channel_name);
	KUNIT_ASSERT_PTR_EQ(test, supplier, &f->bound_provider->dev);
	put_device(supplier);
	KUNIT_EXPECT_PTR_EQ(test, find_pmic_glink_supplier("absent-channel"), NULL);
	device_lock(&f->bound_provider->dev);
	KUNIT_EXPECT_EQ(test, PTR_ERR(pmic_glink_link_provider(f->parent,
		&f->bound_provider->dev, f->provider.channel_name)), -EPROBE_DEFER);
	device_unlock(&f->bound_provider->dev);
	KUNIT_EXPECT_EQ(test, PTR_ERR(pmic_glink_link_provider(f->parent,
		&f->bound_provider->dev, "wrong-channel")), -ENODEV);
	f->bound_consumer = platform_device_register_data(NULL, "avr-glink-link-unit",
		PLATFORM_DEVID_AUTO, &f, sizeof(f));
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->bound_consumer));
	KUNIT_ASSERT_TRUE(test, device_is_bound(&f->bound_consumer->dev));
	KUNIT_EXPECT_PTR_EQ(test, f->linked_provider, &f->provider);
	device_release_driver(&f->bound_provider->dev);
	KUNIT_EXPECT_FALSE(test, device_is_bound(&f->bound_consumer->dev));
	KUNIT_EXPECT_EQ(test, f->consumer_removed_at, 1U);
	KUNIT_EXPECT_EQ(test, f->provider_removed_at, 2U);
	KUNIT_EXPECT_TRUE(test, f->provider_stopped);
	KUNIT_EXPECT_PTR_EQ(test, dev_get_drvdata(&f->transport.dev), NULL);
	KUNIT_EXPECT_PTR_EQ(test, find_pmic_glink_supplier(f->provider.channel_name), NULL);
	KUNIT_EXPECT_EQ(test, PTR_ERR(pmic_glink_link_provider(f->parent,
		&f->bound_provider->dev, f->provider.channel_name)), -EPROBE_DEFER);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(registration_and_framing), KUNIT_CASE(dispatch_and_state),
	KUNIT_CASE(duplicate_owner_rejected), KUNIT_CASE(self_unregister_rx_rejected),
	KUNIT_CASE(self_unregister_state_rejected), KUNIT_CASE(unregister_waits_for_rx),
	KUNIT_CASE(detached_transport_rejects_rx), KUNIT_CASE(detach_waits_for_ingress_and_rx),
	KUNIT_CASE(detach_waits_for_ingress_without_work),
	KUNIT_CASE(linked_provider_unbind_orders_transport_first), {}
};
static struct kunit_suite suite = { .name = "avr-pmic-glink", .init = setup,
	.exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
