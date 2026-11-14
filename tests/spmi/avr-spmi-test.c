// SPDX-License-Identifier: GPL-2.0-only
/* Isolated RAM regmap + recording SPMI controller: no physical PMIC accesses. */
#include <kunit/test.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/spmi.h>
#include <linux/qti-regmap-debugfs.h>
#include <linux/soc/qcom/spmi-pmic-arb.h>

static struct device *ram_device;
static struct regmap *ram_map;
static u32 registers[16] = { 0x13572468 };
static bool debug_attached;
static DEFINE_MUTEX(fixture_lock);
static int ram_read(void *ctx, unsigned int reg, unsigned int *val)
{
	if (reg % 4 || reg >= sizeof(registers))
		return -EINVAL;
	*val = registers[reg / 4];
	return 0;
}
static int ram_write(void *ctx, unsigned int reg, unsigned int val)
{
	if (reg % 4 || reg >= sizeof(registers))
		return -EINVAL;
	registers[reg / 4] = val;
	return 0;
}
static const struct regmap_bus ram_bus = { .reg_read = ram_read, .reg_write = ram_write };
static const struct regmap_config config = {
	.name = "ram", .reg_bits = 32, .val_bits = 32, .reg_stride = 4,
	.max_register = 0x3c, .cache_type = REGCACHE_NONE,
};
static int drop_debug(const char *val, const struct kernel_param *kp)
{
	bool requested;
	int ret = kstrtobool(val, &requested);
	if (ret || !requested)
		return ret ? ret : -EINVAL;
	mutex_lock(&fixture_lock);
	if (!ram_map) {
		ret = -ENODEV;
	} else if (debug_attached) {
		devm_regmap_qti_debugfs_unregister(ram_map);
		debug_attached = false;
	}
	mutex_unlock(&fixture_lock);
	return ret;
}
static const struct kernel_param_ops drop_ops = { .set = drop_debug };
module_param_cb(drop_debug, &drop_ops, NULL, 0200);

struct transaction {
	struct spmi_device *sdev;
	int status;
	unsigned int calls;
	u8 opcode, sid;
	u16 addr;
	size_t len;
};
static int record_read(struct spmi_controller *ctrl, u8 opcode, u8 sid,
		       u16 addr, u8 *buf, size_t len)
{
	struct transaction *t = spmi_controller_get_drvdata(ctrl);
	size_t i;
	t->calls++; t->opcode = opcode; t->sid = sid; t->addr = addr; t->len = len;
	if (!t->status)
		for (i = 0; i < len; i++)
			buf[i] = 0xa0 + i;
	return t->status;
}
static int record_write(struct spmi_controller *ctrl, u8 opcode, u8 sid,
			u16 addr, const u8 *buf, size_t len)
{
	struct transaction *t = spmi_controller_get_drvdata(ctrl);
	t->calls++; t->opcode = opcode; t->sid = sid; t->addr = addr; t->len = len;
	return t->status;
}
static int setup(struct kunit *test)
{
	struct spmi_controller *ctrl = spmi_controller_alloc(ram_device, sizeof(struct transaction));
	struct transaction *t;
	if (IS_ERR(ctrl))
		return PTR_ERR(ctrl);
	t = spmi_controller_get_drvdata(ctrl);
	t->sdev = spmi_device_alloc(ctrl);
	if (!t->sdev) {
		spmi_controller_put(ctrl);
		return -ENOMEM;
	}
	ctrl->read_cmd = record_read; ctrl->write_cmd = record_write;
	t->sdev->usid = 7;
	test->priv = ctrl;
	return 0;
}
static void cleanup(struct kunit *test)
{
	struct spmi_controller *ctrl = test->priv;
	struct transaction *t = spmi_controller_get_drvdata(ctrl);
	spmi_device_put(t->sdev);
	spmi_controller_put(ctrl);
}
static void regmap_roundtrip(struct kunit *test)
{
	unsigned int value = 0;
	KUNIT_EXPECT_EQ(test, regmap_write(ram_map, 0x10, 0x739201a5), 0);
	KUNIT_EXPECT_EQ(test, regmap_read(ram_map, 0x10, &value), 0);
	KUNIT_EXPECT_EQ(test, value, 0x739201a5U);
}
static void regmap_constraints(struct kunit *test)
{
	unsigned int value;
	KUNIT_EXPECT_LT(test, regmap_read(ram_map, 0x40, &value), 0);
	KUNIT_EXPECT_LT(test, regmap_write(ram_map, 1, 0x123), 0);
	KUNIT_EXPECT_EQ(test, registers[0], 0x13572468U);
}
static void mapping_argument_validation(struct kunit *test)
{
	struct resource res = {};
	struct device consumer = {};
	KUNIT_EXPECT_EQ(test, spmi_pmic_arb_map_address(NULL, 0, &res), -EINVAL);
	KUNIT_EXPECT_EQ(test, spmi_pmic_arb_map_address(ram_device, 0, NULL), -EINVAL);
	consumer.of_node = of_find_node_by_path("/");
	KUNIT_ASSERT_NOT_NULL(test, consumer.of_node);
	KUNIT_EXPECT_EQ(test, spmi_pmic_arb_map_address(&consumer, BIT(21), &res), -EINVAL);
	KUNIT_EXPECT_EQ(test, spmi_pmic_arb_map_address(&consumer, 0, &res), -ENODEV);
	of_node_put(consumer.of_node);
}
static void native_spmi_transaction(struct kunit *test)
{
	struct spmi_controller *ctrl = test->priv;
	struct transaction *t = spmi_controller_get_drvdata(ctrl);
	u8 buf[8] = {};
	int i;
	KUNIT_EXPECT_EQ(test, spmi_ext_register_readl(t->sdev, 0x1234, buf, sizeof(buf)), 0);
	KUNIT_EXPECT_EQ(test, t->calls, 1U);
	KUNIT_EXPECT_EQ(test, t->opcode, (u8)SPMI_CMD_EXT_READL);
	KUNIT_EXPECT_EQ(test, t->sid, (u8)7);
	KUNIT_EXPECT_EQ(test, t->addr, (u16)0x1234);
	KUNIT_EXPECT_EQ(test, t->len, (size_t)8);
	for (i = 0; i < 8; i++)
		KUNIT_EXPECT_EQ(test, buf[i], (u8)(0xa0 + i));
}
static void native_spmi_error_and_limits(struct kunit *test)
{
	struct spmi_controller *ctrl = test->priv;
	struct transaction *t = spmi_controller_get_drvdata(ctrl);
	u8 buf[9] = {};
	t->status = -ETIMEDOUT;
	KUNIT_EXPECT_EQ(test, spmi_ext_register_readl(t->sdev, 0x5678, buf, 4), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, spmi_ext_register_readl(t->sdev, 0, buf, 0), -EINVAL);
	KUNIT_EXPECT_EQ(test, spmi_ext_register_readl(t->sdev, 0, buf, 9), -EINVAL);
	KUNIT_EXPECT_EQ(test, t->calls, 1U);
	KUNIT_EXPECT_EQ(test, spmi_ext_register_writel(t->sdev, 0x5678, buf, 4), -ETIMEDOUT);
	KUNIT_EXPECT_EQ(test, t->opcode, (u8)SPMI_CMD_EXT_WRITEL);
	KUNIT_EXPECT_EQ(test, t->calls, 2U);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(regmap_roundtrip), KUNIT_CASE(regmap_constraints),
	KUNIT_CASE(mapping_argument_validation), KUNIT_CASE(native_spmi_transaction),
	KUNIT_CASE(native_spmi_error_and_limits), {}
};
static struct kunit_suite suite = {
	.name = "avr-spmi", .init = setup, .exit = cleanup, .test_cases = cases,
};
kunit_test_suite(suite);
static int __init fixture_init(void)
{
	int ret;
	ram_device = root_device_register("avr_spmi_ram");
	if (IS_ERR(ram_device))
		return PTR_ERR(ram_device);
	ram_map = regmap_init(ram_device, &ram_bus, NULL, &config);
	if (IS_ERR(ram_map)) {
		ret = PTR_ERR(ram_map); ram_map = NULL;
		goto remove_device;
	}
	ret = devm_regmap_qti_debugfs_register(ram_device, ram_map);
	if (ret) {
		regmap_exit(ram_map); ram_map = NULL;
		goto remove_device;
	}
	debug_attached = true;
	return 0;
remove_device:
	root_device_unregister(ram_device);
	return ret;
}
static void __exit fixture_exit(void)
{
	if (debug_attached)
		devm_regmap_qti_debugfs_unregister(ram_map);
	regmap_exit(ram_map);
	ram_map = NULL;
	root_device_unregister(ram_device);
}
module_init(fixture_init);
module_exit(fixture_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPMI framework and actual QTI debugfs lifetime tests using RAM only");
