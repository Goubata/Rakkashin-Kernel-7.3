// SPDX-License-Identifier: GPL-2.0-only
/* Exact protocol and SMEM callback sources on guarded RAM. Doorbell recording
 * replaces mailbox I/O only in this test; no Qualcomm provider is registered.
 */
#include <kunit/test.h>
#include <linux/kthread.h>
#include "../../ports/qcom-glink-transport/qcom_glink_native.c"
#include "../../ports/qcom-glink-transport/qcom_glink_smem.c"
struct fixture {
	struct glink_smem_pipe rx, tx;
	__le32 rx_head, rx_tail, tx_head, tx_tail;
	u8 rx_guarded[80], tx_guarded[80];
	unsigned int kicks;
	struct device *dev;
	struct device_node *node;
	struct qcom_glink *core;
	struct completion removed;
};
static void record_kick(struct qcom_glink_pipe *pipe)
{
	struct glink_smem_pipe *tx = to_smem_pipe(pipe);
	struct fixture *f = container_of(tx, struct fixture, tx);
	f->kicks++;
}
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	unsigned int i;
	if (!f)
		return -ENOMEM;
	memset(f->rx_guarded, 0xa5, sizeof(f->rx_guarded));
	memset(f->tx_guarded, 0xa5, sizeof(f->tx_guarded));
	f->rx.fifo = f->rx_guarded + 8; f->tx.fifo = f->tx_guarded + 8;
	for (i = 0; i < 64; i++) ((u8 *)f->rx.fifo)[i] = i;
	f->rx.head = &f->rx_head; f->rx.tail = &f->rx_tail;
	f->tx.head = &f->tx_head; f->tx.tail = &f->tx_tail;
	f->rx.native.length = f->tx.native.length = 64;
	f->rx.native.avail = glink_smem_rx_avail;
	f->rx.native.peek = glink_smem_rx_peek;
	f->rx.native.advance = glink_smem_rx_advance;
	f->tx.native.avail = glink_smem_tx_avail;
	f->tx.native.write = glink_smem_tx_write;
	f->tx.native.kick = record_kick;
	f->dev = root_device_register("avr_glink_pipe_unit");
	if (IS_ERR(f->dev))
		return PTR_ERR(f->dev);
	f->node = of_find_node_by_path("/");
	if (!f->node) {
		root_device_unregister(f->dev);
		return -ENODEV;
	}
	f->dev->of_node = f->node;
	init_completion(&f->removed);
	test->priv = f;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	if (!IS_ERR_OR_NULL(f->core))
		qcom_glink_native_remove(f->core);
	root_device_unregister(f->dev);
	of_node_put(f->node);
}
static void rx_wrap(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 bytes[24];
	unsigned int i;
	f->rx_head = cpu_to_le32(16); f->rx_tail = cpu_to_le32(48);
	KUNIT_EXPECT_EQ(test, glink_smem_rx_avail(&f->rx.native), (size_t)32);
	glink_smem_rx_peek(&f->rx.native, bytes, 8, sizeof(bytes));
	for (i = 0; i < sizeof(bytes); i++)
		KUNIT_EXPECT_EQ(test, bytes[i], (u8)((56+i)%64));
	glink_smem_rx_advance(&f->rx.native, 32);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->rx_tail), 16U);
}
static void tx_wrap_and_padding(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 header[8] = {1,2,3,4,5,6,7,8}, payload[12];
	memset(payload, 0x63, sizeof(payload));
	f->tx_head = cpu_to_le32(56); f->tx_tail = cpu_to_le32(48);
	KUNIT_EXPECT_EQ(test, glink_smem_tx_avail(&f->tx.native), (size_t)40);
	glink_smem_tx_write(&f->tx.native, header, sizeof(header), payload, sizeof(payload));
	KUNIT_EXPECT_MEMEQ(test, (u8 *)f->tx.fifo+56, header, sizeof(header));
	KUNIT_EXPECT_MEMEQ(test, f->tx.fifo, payload, sizeof(payload));
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->tx_head), 16U);
	KUNIT_EXPECT_EQ(test, f->tx_guarded[7], (u8)0xa5);
	KUNIT_EXPECT_EQ(test, f->tx_guarded[72], (u8)0xa5);
}
static void read_notification_reserve(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 header[8] = {13,0,0,0,0,0,0,0};
	f->tx_head = cpu_to_le32(48);
	KUNIT_EXPECT_EQ(test, glink_smem_tx_avail(&f->tx.native), (size_t)0);
	/* The extra 8-byte reserve is for READ_NOTIF even when normal space is 0. */
	glink_smem_tx_write(&f->tx.native, header, sizeof(header), NULL, 0);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->tx_head), 56U);
	KUNIT_EXPECT_MEMEQ(test, (u8 *)f->tx.fifo+48, header, sizeof(header));
}
static void invalid_ring_positions(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->rx_head = cpu_to_le32(80);
	KUNIT_EXPECT_EQ(test, glink_smem_rx_avail(&f->rx.native), (size_t)0);
	f->rx_head = 0; f->rx_tail = cpu_to_le32(80);
	KUNIT_EXPECT_EQ(test, glink_smem_rx_avail(&f->rx.native), (size_t)0);
	f->tx_head = cpu_to_le32(80);
	KUNIT_EXPECT_EQ(test, glink_smem_tx_avail(&f->tx.native), (size_t)0);
}
static int count_bound_child(struct device *dev, void *data)
{
	unsigned int *count = data;
	if (device_is_bound(dev)) (*count)++;
	return 0;
}
static void probe_core(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->core = qcom_glink_native_probe(f->dev, GLINK_FEATURE_INTENT_REUSE,
		&f->rx.native, &f->tx.native, false);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->core));
}
static void version_wire_and_control_driver(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 expected[8] = {0,0,1,0,1,0,0,0};
	unsigned int children = 0;
	probe_core(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->core));
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->tx_head), 8U);
	KUNIT_EXPECT_MEMEQ(test, f->tx.fifo, expected, sizeof(expected));
	KUNIT_EXPECT_EQ(test, f->kicks, 1U);
	device_for_each_child(f->dev, &children, count_bound_child);
	KUNIT_EXPECT_EQ(test, children, 1U);
}
static void peer_version_ack(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 version[8] = {0,0,1,0,1,0,0,0};
	u8 ack[8] = {1,0,1,0,1,0,0,0};
	probe_core(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->core));
	memcpy(f->rx.fifo, version, sizeof(version)); f->rx_head = cpu_to_le32(8);
	qcom_glink_native_rx(f->core);
	flush_work(&f->core->rx_work);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->rx_tail), 8U);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->tx_head), 16U);
	KUNIT_EXPECT_MEMEQ(test, (u8 *)f->tx.fifo+8, ack, sizeof(ack));
}
static void invalid_access_does_not_touch_ram(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 output[8], before[64];
	memset(output, 0x6a, sizeof(output));
	f->rx_head = cpu_to_le32(16);
	glink_smem_rx_peek(&f->rx.native, output, 16, sizeof(output));
	KUNIT_EXPECT_EQ(test, output[0], (u8)0x6a);
	glink_smem_rx_peek(&f->rx.native, output, UINT_MAX, 1);
	KUNIT_EXPECT_EQ(test, output[0], (u8)0x6a);
	glink_smem_rx_advance(&f->rx.native, 24);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->rx_tail), 0U);
	glink_smem_rx_advance(&f->rx.native, 1);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->rx_tail), 0U);
	memcpy(before, f->tx.fifo, sizeof(before));
	glink_smem_tx_write(&f->tx.native, output, SIZE_MAX, NULL, 0);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->tx_head), 0U);
	KUNIT_EXPECT_MEMEQ(test, f->tx.fifo, before, sizeof(before));
	glink_smem_tx_write(&f->tx.native, output, sizeof(output), NULL, 0);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->tx_head), 8U);
}
static void oversized_core_packet_rejected(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 header[8] = {};
	probe_core(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->core));
	KUNIT_EXPECT_EQ(test, qcom_glink_tx(f->core, header, SIZE_MAX, NULL, 1, false), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_glink_tx(f->core, header, sizeof(header), header, SIZE_MAX, false), -EINVAL);
	KUNIT_EXPECT_EQ(test, le32_to_cpu(f->tx_head), 8U);
}
static void invalid_registration_rejected(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, PTR_ERR(qcom_glink_smem_register(NULL, NULL)), -EINVAL);
}
static int remove_core_thread(void *data)
{
	struct fixture *f = data;
	qcom_glink_native_remove(f->core);
	complete(&f->removed);
	while (!kthread_should_stop())
		schedule_timeout_interruptible(HZ);
	return 0;
}
static void removal_aborts_blocked_response(struct kunit *test)
{
	struct fixture *f = test->priv;
	u8 version[8] = {0,0,1,0,1,0,0,0};
	struct task_struct *thread;
	unsigned long deadline, completed;
	probe_core(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->core));
	/* Fill ordinary TX capacity; the RX worker must wait sending VERSION_ACK. */
	f->tx_head = cpu_to_le32(48); f->tx_tail = 0;
	memcpy(f->rx.fifo, version, sizeof(version)); f->rx_head = cpu_to_le32(8);
	qcom_glink_native_rx(f->core);
	deadline = jiffies + msecs_to_jiffies(1000);
	while (!READ_ONCE(f->core->sent_read_notify) && time_before(jiffies, deadline))
		usleep_range(100, 200);
	if (!READ_ONCE(f->core->sent_read_notify)) {
		f->tx_tail = f->tx_head;
		wake_up_all(&f->core->tx_avail_notify);
		flush_work(&f->core->rx_work);
		KUNIT_FAIL(test, "response worker did not block");
		return;
	}
	thread = kthread_run(remove_core_thread, f, "avr_glink_remove");
	if (IS_ERR(thread)) {
		f->tx_tail = f->tx_head;
		wake_up_all(&f->core->tx_avail_notify);
		flush_work(&f->core->rx_work);
		KUNIT_FAIL(test, "remove thread allocation failed");
		return;
	}
	completed = wait_for_completion_timeout(&f->removed, msecs_to_jiffies(500));
	if (!completed) {
		/* Recovery only for the pre-fix regression: let the old waiter exit. */
		WRITE_ONCE(f->tx_tail, READ_ONCE(f->tx_head));
		wake_up_all(&f->core->tx_avail_notify);
		wait_for_completion(&f->removed);
	}
	kthread_stop(thread);
	f->core = NULL;
	KUNIT_EXPECT_NE(test, completed, 0UL);
}
static void unsupported_vendor_feature_not_negotiated(struct kunit *test)
{
	struct fixture *f = test->priv;
	/* Vendor peer offers reuse | zero-copy (bit 3). We implement reuse only. */
	u8 version[8] = {0,0,1,0,9,0,0,0};
	u8 ack[8] = {1,0,1,0,1,0,0,0};
	probe_core(test);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(f->core));
	memcpy(f->rx.fifo, version, sizeof(version)); f->rx_head = cpu_to_le32(8);
	qcom_glink_native_rx(f->core);
	flush_work(&f->core->rx_work);
	KUNIT_EXPECT_EQ(test, f->core->features, (unsigned long)GLINK_FEATURE_INTENT_REUSE);
	KUNIT_EXPECT_MEMEQ(test, (u8 *)f->tx.fifo+8, ack, sizeof(ack));
}
static struct kunit_case cases[] = {
	KUNIT_CASE(rx_wrap), KUNIT_CASE(tx_wrap_and_padding),
	KUNIT_CASE(read_notification_reserve), KUNIT_CASE(invalid_ring_positions),
	KUNIT_CASE(version_wire_and_control_driver), KUNIT_CASE(peer_version_ack),
	KUNIT_CASE(invalid_access_does_not_touch_ram), KUNIT_CASE(oversized_core_packet_rejected),
	KUNIT_CASE(invalid_registration_rejected), KUNIT_CASE(removal_aborts_blocked_response),
	KUNIT_CASE(unsupported_vendor_feature_not_negotiated), {}
};
static struct kunit_suite suite = { .name = "avr-glink-transport",
	.init = setup, .exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
