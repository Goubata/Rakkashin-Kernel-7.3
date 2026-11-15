// SPDX-License-Identifier: GPL-2.0-only
/* Real built-in API with real DT lookup/client references and concurrent tasks. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/soc/qcom/smem_state.h>
struct fixture {
	struct device *client;
	struct device_node *provider, *bad;
	struct qcom_smem_state *owner, *held;
	struct completion entered, release, removing, removed;
	bool block;
	u32 calls;
	int update_result;
};
static int update(void *priv, u32 mask, u32 value)
{
	struct fixture *f = priv;
	f->calls++;
	if (f->block) {
		complete(&f->entered);
		wait_for_completion(&f->release);
	}
	return 0;
}
static const struct qcom_smem_state_ops ops = { .update_bits = update };
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	struct device_node *client;
	if (!f) return -ENOMEM;
	f->provider = of_find_node_by_path("/avr-qemu-state-provider");
	f->bad = of_find_node_by_path("/avr-qemu-state-bad-client");
	client = of_find_node_by_path("/avr-qemu-state-client");
	if (!f->provider || !f->bad || !client) {
		of_node_put(f->provider); of_node_put(f->bad); of_node_put(client);
		return -ENODEV;
	}
	f->client = root_device_register("avr_smem_state_client");
	if (IS_ERR(f->client)) {
		of_node_put(f->provider); of_node_put(f->bad); of_node_put(client);
		return PTR_ERR(f->client);
	}
	f->client->of_node = client;
	init_completion(&f->entered); init_completion(&f->release);
	init_completion(&f->removing); init_completion(&f->removed);
	test->priv = f;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	if (!IS_ERR_OR_NULL(f->owner)) qcom_smem_state_unregister(f->owner);
	if (!IS_ERR_OR_NULL(f->held)) qcom_smem_state_put(f->held);
	of_node_put(f->client->of_node);
	root_device_unregister(f->client);
	of_node_put(f->provider); of_node_put(f->bad);
}
static int writer(void *data)
{
	struct fixture *f = data;
	f->update_result = qcom_smem_state_update_bits(f->held, 8, 8);
	return 0;
}
static int remover(void *data)
{
	struct fixture *f = data;
	complete(&f->removing);
	qcom_smem_state_unregister(f->owner);
	complete(&f->removed);
	return 0;
}
static void unregister_drains_callback(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct task_struct *w, *r;
	unsigned int bit;
	unsigned long entered, started, early;
	f->owner = qcom_smem_state_register(f->provider, &ops, f);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->owner));
	f->held = qcom_smem_state_get(f->client, NULL, &bit);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->held));
	KUNIT_EXPECT_EQ(test, bit, 3U);
	f->block = true;
	w = kthread_run(writer, f, "avr_state_writer");
	KUNIT_ASSERT_FALSE(test, IS_ERR(w));
	entered = wait_for_completion_timeout(&f->entered, HZ);
	if (!entered) {
		complete(&f->release); kthread_stop(w);
		KUNIT_FAIL(test, "writer did not enter callback"); return;
	}
	r = kthread_run(remover, f, "avr_state_remover");
	if (IS_ERR(r)) {
		complete(&f->release); kthread_stop(w);
		KUNIT_FAIL(test, "cannot start remover"); return;
	}
	started = wait_for_completion_timeout(&f->removing, HZ);
	early = wait_for_completion_timeout(&f->removed, msecs_to_jiffies(50));
	complete(&f->release);
	if (!early) wait_for_completion(&f->removed);
	kthread_stop(w); kthread_stop(r); f->owner = NULL;
	KUNIT_EXPECT_NE(test, started, 0UL);
	KUNIT_EXPECT_EQ(test, early, 0UL);
	KUNIT_EXPECT_EQ(test, f->update_result, 0);
	KUNIT_EXPECT_EQ(test, qcom_smem_state_update_bits(f->held, 8, 0), -ENXIO);
	KUNIT_EXPECT_EQ(test, f->calls, 1U);
}
static void orphan_lookup_and_republish(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct qcom_smem_state *looked;
	unsigned int bit;
	f->owner = qcom_smem_state_register(f->provider, &ops, f);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->owner));
	f->held = qcom_smem_state_get(f->client, NULL, &bit);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->held));
	qcom_smem_state_unregister(f->owner); f->owner = NULL;
	looked = qcom_smem_state_get(f->client, NULL, &bit);
	KUNIT_EXPECT_PTR_EQ(test, looked, ERR_PTR(-EPROBE_DEFER));
	if (!IS_ERR(looked)) qcom_smem_state_put(looked);
	f->owner = qcom_smem_state_register(f->provider, &ops, f);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->owner));
	looked = qcom_smem_state_get(f->client, NULL, &bit);
	KUNIT_ASSERT_FALSE(test, IS_ERR(looked));
	KUNIT_EXPECT_PTR_EQ(test, looked, f->owner);
	KUNIT_EXPECT_EQ(test, qcom_smem_state_update_bits(f->held, 8, 8), -ENXIO);
	KUNIT_EXPECT_EQ(test, qcom_smem_state_update_bits(looked, 8, 8), 0);
	qcom_smem_state_put(looked);
}
static void state_bit_range(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct device_node *saved = f->client->of_node;
	struct qcom_smem_state *looked;
	unsigned int bit = 99;
	f->owner = qcom_smem_state_register(f->provider, &ops, f);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->owner));
	f->client->of_node = f->bad;
	looked = qcom_smem_state_get(f->client, NULL, &bit);
	f->client->of_node = saved;
	KUNIT_EXPECT_PTR_EQ(test, looked, ERR_PTR(-EINVAL));
	if (!IS_ERR(looked)) qcom_smem_state_put(looked);
	KUNIT_EXPECT_EQ(test, bit, 99U);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(unregister_drains_callback), KUNIT_CASE(orphan_lookup_and_republish),
	KUNIT_CASE(state_bit_range), {}
};
static struct kunit_suite suite = { .name = "avr-smem-state", .init = setup, .exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
