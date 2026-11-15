// SPDX-License-Identifier: GPL-2.0-only
/* Production callbacks against RAM registers; no physical IPCC probe. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/mailbox_client.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(...)
#include "../../ports/qcom-ipcc/qcom-ipcc.c"
struct fixture {
	struct qcom_ipcc ipcc;
	struct mbox_chan chans[3];
	struct qcom_ipcc_chan_info info[3];
	u32 regs[32];
};
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	int i;
	if (!f) return -ENOMEM;
	f->ipcc.base = (void __iomem *)f->regs;
	f->ipcc.chans = f->chans;
	f->ipcc.mchan = f->info;
	f->ipcc.mbox.chans = f->chans;
	f->ipcc.mbox.num_chans = 3;
	spin_lock_init(&f->ipcc.channel_lock);
	for (i = 0; i < 3; i++) f->chans[i].mbox = &f->ipcc.mbox;
	test->priv = f;
	return 0;
}
static struct mbox_chan *request(struct fixture *f, u32 client, u32 signal)
{
	struct of_phandle_args args = { .args_count = 2, .args = {client, signal} };
	return qcom_ipcc_mbox_xlate(&f->ipcc.mbox, &args);
}
static void irq_translation_bounds(struct kunit *test)
{
	u32 spec[3] = {13, 0, IRQ_TYPE_EDGE_RISING};
	unsigned long hw = 123;
	unsigned int type = 321;
	KUNIT_ASSERT_EQ(test, qcom_ipcc_domain_xlate(NULL, NULL, spec, 3, &hw, &type), 0);
	KUNIT_EXPECT_EQ(test, hw, 0xd0000UL);
	KUNIT_EXPECT_EQ(test, type, (unsigned int)IRQ_TYPE_EDGE_RISING);
	spec[0] = 0x1000d;
	KUNIT_EXPECT_EQ(test, qcom_ipcc_domain_xlate(NULL, NULL, spec, 3, &hw, &type), -EINVAL);
	spec[0] = 13; spec[1] = 0x10000;
	KUNIT_EXPECT_EQ(test, qcom_ipcc_domain_xlate(NULL, NULL, spec, 3, &hw, &type), -EINVAL);
	spec[0] = spec[1] = U16_MAX;
	KUNIT_EXPECT_EQ(test, qcom_ipcc_domain_xlate(NULL, NULL, spec, 3, &hw, &type), -EINVAL);
	KUNIT_EXPECT_EQ(test, qcom_ipcc_domain_xlate(NULL, NULL, spec, 2, &hw, &type), -EINVAL);
	KUNIT_EXPECT_EQ(test, hw, 0xd0000UL);
}
static void mailbox_translation_bounds(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct of_phandle_args args = { .args_count = 1, .args = {13,0} };
	KUNIT_EXPECT_PTR_EQ(test, qcom_ipcc_mbox_xlate(&f->ipcc.mbox, &args), ERR_PTR(-EINVAL));
	KUNIT_EXPECT_PTR_EQ(test, request(f, 0x1000d, 0), ERR_PTR(-EINVAL));
	KUNIT_EXPECT_PTR_EQ(test, request(f, 13, 0x10000), ERR_PTR(-EINVAL));
	KUNIT_EXPECT_PTR_EQ(test, request(f, U16_MAX, U16_MAX), ERR_PTR(-EINVAL));
	KUNIT_EXPECT_PTR_EQ(test, request(f, U16_MAX, U16_MAX - 1), &f->chans[0]);
}
static void duplicate_after_hole(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_ASSERT_PTR_EQ(test, request(f, 12, 0), &f->chans[0]);
	KUNIT_ASSERT_PTR_EQ(test, request(f, 13, 0), &f->chans[1]);
	qcom_ipcc_mbox_shutdown(&f->chans[0]);
	KUNIT_EXPECT_PTR_EQ(test, request(f, 13, 0), ERR_PTR(-EBUSY));
	KUNIT_EXPECT_PTR_EQ(test, request(f, 14, 0), &f->chans[0]);
}
static void repeated_release_reuses_storage(struct kunit *test)
{
	struct fixture *f = test->priv;
	int i;
	for (i = 0; i < 1000; i++) {
		struct mbox_chan *chan = request(f, 13, i);
		KUNIT_ASSERT_PTR_EQ(test, chan, &f->chans[0]);
		KUNIT_ASSERT_PTR_EQ(test, chan->con_priv, &f->info[0]);
		KUNIT_EXPECT_EQ(test, f->info[0].signal_id, (u16)i);
		qcom_ipcc_mbox_shutdown(chan);
		KUNIT_EXPECT_PTR_EQ(test, chan->con_priv, NULL);
	}
}
static void channel_exhaustion(struct kunit *test)
{
	struct fixture *f = test->priv;
	int i;
	for (i = 0; i < 3; i++) KUNIT_ASSERT_PTR_EQ(test, request(f, 13, i), &f->chans[i]);
	KUNIT_EXPECT_PTR_EQ(test, request(f, 13, 3), ERR_PTR(-EBUSY));
	qcom_ipcc_mbox_shutdown(&f->chans[1]);
	KUNIT_EXPECT_PTR_EQ(test, request(f, 13, 3), &f->chans[1]);
}
static void send_mask_unmask_registers(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct mbox_chan *chan = request(f, 13, 2);
	struct irq_data irq = { .hwirq = 0xd0002, .chip_data = &f->ipcc };
	KUNIT_ASSERT_FALSE(test, IS_ERR(chan));
	KUNIT_EXPECT_EQ(test, qcom_ipcc_mbox_send_data(chan, NULL), 0);
	KUNIT_EXPECT_EQ(test, f->regs[IPCC_REG_SEND_ID/4], 0xd0002U);
	qcom_ipcc_mask_irq(&irq);
	KUNIT_EXPECT_EQ(test, f->regs[IPCC_REG_RECV_SIGNAL_DISABLE/4], 0xd0002U);
	qcom_ipcc_unmask_irq(&irq);
	KUNIT_EXPECT_EQ(test, f->regs[IPCC_REG_RECV_SIGNAL_ENABLE/4], 0xd0002U);
	KUNIT_EXPECT_EQ(test, f->regs[IPCC_REG_CONFIG/4], 0U);
}
static void released_but_bound_slot_not_reused(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct mbox_client client = {};
	KUNIT_ASSERT_PTR_EQ(test, request(f, 13, 0), &f->chans[0]);
	/* The mailbox core clears cl after invoking shutdown. */
	f->chans[0].cl = &client;
	qcom_ipcc_mbox_shutdown(&f->chans[0]);
	KUNIT_EXPECT_PTR_EQ(test, request(f, 14, 0), &f->chans[1]);
	f->chans[0].cl = NULL;
	KUNIT_EXPECT_PTR_EQ(test, request(f, 15, 0), &f->chans[0]);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(irq_translation_bounds), KUNIT_CASE(mailbox_translation_bounds),
	KUNIT_CASE(duplicate_after_hole), KUNIT_CASE(repeated_release_reuses_storage),
	KUNIT_CASE(channel_exhaustion), KUNIT_CASE(send_mask_unmask_registers),
	KUNIT_CASE(released_but_bound_slot_not_reused), {}
};
static struct kunit_suite suite = { .name = "avr-ipcc", .init = setup, .test_cases = cases };
kunit_test_suite(suite);
