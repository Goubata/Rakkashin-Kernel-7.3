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
	pmic_glink_dev_add(&f->provider);
	test->priv = f;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	complete_all(&f->release);
	flush_workqueue(f->provider.rx_wq);
	if (!IS_ERR_OR_NULL(f->client))
		pmic_glink_unregister_client(f->client);
	pmic_glink_dev_remove(&f->provider);
	destroy_workqueue(f->provider.rx_wq);
	idr_destroy(&f->provider.client_idr);
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
static struct kunit_case cases[] = {
	KUNIT_CASE(registration_and_framing), KUNIT_CASE(dispatch_and_state),
	KUNIT_CASE(duplicate_owner_rejected), KUNIT_CASE(self_unregister_rx_rejected),
	KUNIT_CASE(self_unregister_state_rejected), KUNIT_CASE(unregister_waits_for_rx), {}
};
static struct kunit_suite suite = { .name = "avr-pmic-glink", .init = setup,
	.exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
