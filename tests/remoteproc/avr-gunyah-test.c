// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/gunyah/gh_dbl.h>
#include <linux/gunyah/gh_msgq.h>
#include <linux/gunyah/gh_rm_drv.h>

static void rx_callback(int irq, void *private) {}

static void doorbell_missing_capability(struct kunit *test)
{
	void *tx = gh_dbl_tx_register(GH_DBL_HW_FENCE);
	u64 flags = 1;

	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(tx));
	KUNIT_EXPECT_EQ(test, PTR_ERR(gh_dbl_tx_register(GH_DBL_HW_FENCE)), (long)-EBUSY);
	KUNIT_EXPECT_EQ(test, gh_dbl_send(tx, &flags, GH_DBL_NONBLOCK), -EAGAIN);
	KUNIT_EXPECT_EQ(test, gh_dbl_tx_unregister(tx), 0);
}
static void shared_direction_lifetime(struct kunit *test)
{
	void *tx = gh_dbl_tx_register(GH_DBL_HW_FENCE);
	void *rx;
	u64 clear = ~0ULL;

	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(tx));
	rx = gh_dbl_rx_register(GH_DBL_HW_FENCE, rx_callback, NULL);
	if (IS_ERR_OR_NULL(rx)) {
		gh_dbl_tx_unregister(tx);
		KUNIT_FAIL(test, "RX registration failed");
		return;
	}
	KUNIT_EXPECT_PTR_EQ(test, tx, rx);
	KUNIT_EXPECT_EQ(test, gh_dbl_tx_unregister(tx), 0);
	KUNIT_EXPECT_EQ(test, gh_dbl_read_and_clean(rx, &clear, GH_DBL_NONBLOCK), -EAGAIN);
	KUNIT_EXPECT_EQ(test, gh_dbl_rx_unregister(rx), 0);
	/* A released label must accept a fresh client. */
	tx = gh_dbl_tx_register(GH_DBL_HW_FENCE);
	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(tx));
	KUNIT_EXPECT_EQ(test, gh_dbl_tx_unregister(tx), 0);
}
static void message_queue_missing_capability(struct kunit *test)
{
	void *queue = gh_msgq_register(GH_MSGQ_LABEL_MEMBUF);
	u32 payload = 1;
	size_t received = 0;

	KUNIT_ASSERT_FALSE(test, IS_ERR_OR_NULL(queue));
	KUNIT_EXPECT_EQ(test, PTR_ERR(gh_msgq_register(GH_MSGQ_LABEL_MEMBUF)), (long)-EBUSY);
	KUNIT_EXPECT_EQ(test, gh_msgq_send(queue, &payload, sizeof(payload), GH_MSGQ_NONBLOCK), -EAGAIN);
	KUNIT_EXPECT_EQ(test, gh_msgq_recv(queue, &payload, sizeof(payload), &received, GH_MSGQ_NONBLOCK), -EAGAIN);
	KUNIT_EXPECT_EQ(test, gh_msgq_unregister(queue), 0);
}
static void virtual_irq_lifetime(struct kunit *test)
{
	int first = gh_get_virq(64, 0);
	int second;

	KUNIT_ASSERT_GE(test, first, 64);
	second = gh_get_virq(64, 0);
	if (second < 0) {
		gh_put_virq(first);
		KUNIT_FAIL(test, "Second vIRQ allocation failed");
		return;
	}
	KUNIT_EXPECT_NE(test, first, second);
	KUNIT_EXPECT_EQ(test, gh_put_virq(first), 0);
	KUNIT_EXPECT_EQ(test, gh_put_virq(second), 0);
	KUNIT_EXPECT_EQ(test, gh_put_virq(first), -EINVAL);
}
static void avf_wire_labels(struct kunit *test)
{
	KUNIT_EXPECT_EQ(test, (int)GH_DBL_HW_FENCE, 0x106);
	KUNIT_EXPECT_EQ(test, GUNYAH_QCOM_MIN_MSGQ, 0x101);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(doorbell_missing_capability), KUNIT_CASE(shared_direction_lifetime),
	KUNIT_CASE(message_queue_missing_capability), KUNIT_CASE(virtual_irq_lifetime),
	KUNIT_CASE(avf_wire_labels), {}
};
static struct kunit_suite suite = { .name = "avr-gunyah-local", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Gunyah descriptor and capability guard tests without hypercalls");
