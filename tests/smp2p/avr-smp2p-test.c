// SPDX-License-Identifier: GPL-2.0-only
/* Real state/IRQ callbacks and state framework; isolated RAM and regmap bus. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(...)
#include "../../ports/qcom-smp2p/smp2p.c"
struct fixture {
	struct qcom_smp2p smp;
	struct smp2p_smem_item in, out;
	struct smp2p_entry entry;
	struct device *dev;
	struct device_node *node;
	struct qcom_smem_state *registered;
	u32 writes, reg, value;
};
static int record_write(void *ctx, unsigned int reg, unsigned int val)
{
	struct fixture *f = ctx;
	f->writes++; f->reg = reg; f->value = val;
	return 0;
}
static const struct regmap_bus bus = { .reg_write = record_write };
static const struct regmap_config config = { .reg_bits = 32, .val_bits = 32,
	.reg_stride = 4, .max_register = 0x40, .disable_locking = true };
static int setup(struct kunit *test)
{
	struct fixture *f = kunit_kzalloc(test, sizeof(*f), GFP_KERNEL);
	if (!f) return -ENOMEM;
	f->dev = root_device_register("avr_smp2p_unit");
	if (IS_ERR(f->dev)) return PTR_ERR(f->dev);
	f->node = of_find_node_by_path("/");
	f->smp.dev = f->dev;
	f->smp.in = &f->in; f->smp.out = &f->out;
	f->in.magic = f->out.magic = SMP2P_MAGIC;
	f->in.version = f->out.version = 2;
	f->in.total_entries = f->out.total_entries = SMP2P_MAX_ENTRY;
	f->smp.ipc_regmap = regmap_init(f->dev, &bus, f, &config);
	if (IS_ERR(f->smp.ipc_regmap)) {
		of_node_put(f->node); root_device_unregister(f->dev);
		return PTR_ERR(f->smp.ipc_regmap);
	}
	f->smp.ipc_offset = 0x10; f->smp.ipc_bit = 5;
	INIT_LIST_HEAD(&f->smp.inbound); INIT_LIST_HEAD(&f->smp.outbound);
	f->entry.smp2p = &f->smp; f->entry.name = "slave-kernel";
	spin_lock_init(&f->entry.lock);
	test->priv = f;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct fixture *f = test->priv;
	if (!IS_ERR_OR_NULL(f->registered)) qcom_smem_state_unregister(f->registered);
	regmap_exit(f->smp.ipc_regmap);
	of_node_put(f->node);
	root_device_unregister(f->dev);
}
static void shared_header_bounds(struct kunit *test)
{
	struct fixture *f = test->priv;
	KUNIT_EXPECT_TRUE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)));
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)-1));
	f->in.valid_entries = 17;
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)));
	f->in.valid_entries = 2; f->in.total_entries = 1;
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)));
	f->in.total_entries = 17;
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)));
	f->in.total_entries = 16; f->in.version = 3;
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)));
	f->in.version = 0;
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)));
	f->in.version = 2; f->in.magic = 0;
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_valid_item(&f->in, sizeof(f->in)));
}
static void state_framework_mask_and_doorbell(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->entry.value = &f->out.entries[0].value;
	*f->entry.value = 0xa0;
	f->registered = qcom_smem_state_register(f->node, &smp2p_state_ops, &f->entry);
	KUNIT_ASSERT_FALSE(test, IS_ERR(f->registered));
	KUNIT_EXPECT_EQ(test, qcom_smem_state_update_bits(f->registered, 0xf, 0x103), 0);
	KUNIT_EXPECT_EQ(test, *f->entry.value, 0xa3U);
	KUNIT_EXPECT_EQ(test, f->writes, 1U);
	KUNIT_EXPECT_EQ(test, f->reg, 0x10U);
	KUNIT_EXPECT_EQ(test, f->value, 32U);
	KUNIT_EXPECT_EQ(test, qcom_smem_state_update_bits(f->registered, 0xf, 3), 0);
	KUNIT_EXPECT_EQ(test, f->writes, 1U);
}
static void stable_ssr_does_not_reset_irq_history(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->smp.ssr_ack_enabled = true;
	f->entry.value = &f->in.entries[0].value;
	f->entry.last_value = 8;
	list_add(&f->entry.node, &f->smp.inbound);
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_check_ssr(&f->smp));
	KUNIT_EXPECT_EQ(test, f->entry.last_value, 8U);
	f->in.flags = BIT(SMP2P_FLAGS_RESTART_DONE_BIT);
	KUNIT_EXPECT_TRUE(test, qcom_smp2p_check_ssr(&f->smp));
	KUNIT_EXPECT_EQ(test, f->entry.last_value, 0U);
	qcom_smp2p_do_ssr_ack(&f->smp);
	KUNIT_EXPECT_TRUE(test, f->smp.ssr_ack);
	KUNIT_EXPECT_EQ(test, f->out.flags, (u32)BIT(SMP2P_FLAGS_RESTART_ACK_BIT));
	KUNIT_EXPECT_EQ(test, f->writes, 1U);
	f->entry.last_value = 8;
	KUNIT_EXPECT_FALSE(test, qcom_smp2p_check_ssr(&f->smp));
	KUNIT_EXPECT_EQ(test, f->entry.last_value, 8U);
}
static void vendor_v1_restart_preserves_history(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->smp.ssr_ack_enabled = true;
	f->in.version = 1;
	f->in.flags = BIT(SMP2P_FLAGS_RESTART_DONE_BIT);
	f->entry.value = &f->in.entries[0].value;
	f->entry.last_value = 8;
	list_add(&f->entry.node, &f->smp.inbound);
	KUNIT_EXPECT_TRUE(test, qcom_smp2p_check_ssr(&f->smp));
	KUNIT_EXPECT_EQ(test, f->entry.last_value, 8U);
}
static void version_and_feature_negotiation(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->in.version = 1; f->out.features = 1;
	qcom_smp2p_negotiate(&f->smp);
	KUNIT_EXPECT_EQ(test, f->out.version, (u8)1);
	KUNIT_EXPECT_FALSE(test, f->smp.negotiation_done);
	KUNIT_EXPECT_EQ(test, f->writes, 1U);
	f->in.features = 0;
	qcom_smp2p_negotiate(&f->smp);
	KUNIT_EXPECT_TRUE(test, f->smp.negotiation_done);
	KUNIT_EXPECT_FALSE(test, f->smp.ssr_ack_enabled);
	KUNIT_EXPECT_EQ(test, (unsigned)f->out.features, 0U);
}
static void line_state_queries(struct kunit *test)
{
	struct fixture *f = test->priv;
	struct irq_data irq = { .hwirq = 3, .chip_data = &f->entry };
	bool state = false;
	KUNIT_EXPECT_EQ(test, smp2p_irq_get_irqchip_state(&irq, IRQCHIP_STATE_LINE_LEVEL, &state), -ENODEV);
	f->entry.value = &f->in.entries[0].value; *f->entry.value = 8;
	KUNIT_EXPECT_EQ(test, smp2p_irq_get_irqchip_state(&irq, IRQCHIP_STATE_LINE_LEVEL, &state), 0);
	KUNIT_EXPECT_TRUE(test, state);
	*f->entry.value = 0;
	KUNIT_EXPECT_EQ(test, smp2p_irq_get_irqchip_state(&irq, IRQCHIP_STATE_LINE_LEVEL, &state), 0);
	KUNIT_EXPECT_FALSE(test, state);
	KUNIT_EXPECT_EQ(test, smp2p_irq_get_irqchip_state(&irq, IRQCHIP_STATE_PENDING, &state), -EINVAL);
	KUNIT_EXPECT_EQ(test, smp2p_set_irq_type(&irq, IRQ_TYPE_EDGE_BOTH), 0);
	KUNIT_EXPECT_TRUE(test, test_bit(3, f->entry.irq_rising));
	KUNIT_EXPECT_TRUE(test, test_bit(3, f->entry.irq_falling));
	smp2p_unmask_irq(&irq); KUNIT_EXPECT_TRUE(test, test_bit(3, f->entry.irq_enabled));
	smp2p_mask_irq(&irq); KUNIT_EXPECT_FALSE(test, test_bit(3, f->entry.irq_enabled));
}
static void inbound_names_and_fixed_array_bounds(struct kunit *test)
{
	struct fixture *f = test->priv;
	list_add(&f->entry.node, &f->smp.inbound);
	memset(f->in.entries[0].name, 'x', SMP2P_MAX_ENTRY_NAME);
	strscpy(f->in.entries[15].name, "slave-kernel", SMP2P_MAX_ENTRY_NAME);
	f->in.valid_entries = 0xffff;
	qcom_smp2p_notify_in(&f->smp);
	KUNIT_EXPECT_EQ(test, f->smp.valid_entries, 16U);
	KUNIT_EXPECT_PTR_EQ(test, f->entry.value, &f->in.entries[15].value);
	KUNIT_EXPECT_EQ(test, f->entry.last_value, 0U);
}
static void outbound_capacity_and_vendor_longname(struct kunit *test)
{
	struct fixture *f = test->priv;
	f->out.valid_entries = 16;
	KUNIT_EXPECT_EQ(test, qcom_smp2p_outbound_entry(&f->smp, &f->entry, f->node), -ENOSPC);
	KUNIT_EXPECT_EQ(test, f->out.valid_entries, (u16)16);
	f->out.valid_entries = 0; f->entry.name = "wlan_ep_power_save";
	KUNIT_ASSERT_EQ(test, qcom_smp2p_outbound_entry(&f->smp, &f->entry, f->node), 0);
	f->registered = f->entry.state;
	KUNIT_EXPECT_EQ(test, f->out.valid_entries, (u16)1);
	KUNIT_EXPECT_MEMEQ(test, f->out.entries[0].name, "wlan_ep_power_s", 15);
	KUNIT_EXPECT_EQ(test, f->out.entries[0].name[15], (u8)0);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(shared_header_bounds), KUNIT_CASE(state_framework_mask_and_doorbell),
	KUNIT_CASE(stable_ssr_does_not_reset_irq_history), KUNIT_CASE(version_and_feature_negotiation), KUNIT_CASE(vendor_v1_restart_preserves_history),
	KUNIT_CASE(line_state_queries), KUNIT_CASE(inbound_names_and_fixed_array_bounds),
	KUNIT_CASE(outbound_capacity_and_vendor_longname), {}
};
static struct kunit_suite suite = { .name = "avr-smp2p", .init = setup, .exit = cleanup, .test_cases = cases };
kunit_test_suite(suite);
