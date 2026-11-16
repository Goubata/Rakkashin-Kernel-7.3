// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU-only stand-in for PMIC SDAM nvmem ("qcom,spmi-sdam"): RAM-backed nvmem
 * with DT fixed cells (restart@48/49 like the phone) and an optional preset
 * ("avr,preset" bytes from offset 0). A restart handler (priority 200: after
 * moto_reboot_reason's 255, before qcom_scm 130 and PSCI) prints what the reboot
 * path left in SDAM and in the fake-TZ dload register, and the stage verdict.
 */
#include <linux/module.h>
#include <linux/nvmem-provider.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include "../qcom-tee/avr_fake_tz.h"

#define SDAM_SIZE 0x100
struct sdam { u8 ram[SDAM_SIZE]; struct nvmem_device *nvmem; };
static struct sdam *restart_sdam;
static int expect_r48 = -1, expect_r49 = -1, expect_dload = -1;
module_param(expect_r48, int, 0644);
module_param(expect_r49, int, 0644);
module_param(expect_dload, int, 0644);
static unsigned int sdi_before;

static int sdam_read(void *priv, unsigned int off, void *val, size_t n)
{
	struct sdam *s = priv;

	if (off + n > SDAM_SIZE)
		return -EINVAL;
	memcpy(val, s->ram + off, n);
	return 0;
}

static int sdam_write(void *priv, unsigned int off, void *val, size_t n)
{
	struct sdam *s = priv;

	if (off + n > SDAM_SIZE)
		return -EINVAL;
	memcpy(s->ram + off, val, n);
	return 0;
}

static int avr_restart(struct notifier_block *nb, unsigned long mode, void *cmd)
{
	struct avr_fake_tz_stats st;
	bool ok;

	avr_fake_tz_get_stats(&st);
	pr_emerg("AVR_REBOOT_STATE cmd=%s r48=0x%02x r49=0x%02x dload=0x%x sdi_disables=%u\n",
		 cmd ? (char *)cmd : "(null)", restart_sdam->ram[0x48], restart_sdam->ram[0x49],
		 st.last_io_val, st.sdi_disables - sdi_before);
	ok = restart_sdam->ram[0x48] == expect_r48 && restart_sdam->ram[0x49] == expect_r49 &&
	     st.last_io_val == expect_dload && st.sdi_disables > sdi_before;
	pr_emerg("%s\n", ok ? "AVR_QCOM_REBOOT_TESTS_PASS scope=reason-sdam-dload-sdi-fake-tz"
			    : "AVR_QCOM_REBOOT_STATE_FAIL");
	return NOTIFY_DONE;
}
static struct notifier_block restart_nb = { .notifier_call = avr_restart, .priority = 200 };

static int sdi_snapshot_set(const char *v, const struct kernel_param *kp)
{
	struct avr_fake_tz_stats st;

	avr_fake_tz_get_stats(&st);
	sdi_before = st.sdi_disables;
	return 0;
}
static const struct kernel_param_ops sdi_ops = { .set = sdi_snapshot_set };
module_param_cb(snapshot_sdi, &sdi_ops, NULL, 0200);

static int avr_sdam_probe(struct platform_device *pdev)
{
	struct nvmem_config cfg = {
		.dev = &pdev->dev, .name = dev_name(&pdev->dev), .id = NVMEM_DEVID_NONE,
		.size = SDAM_SIZE, .word_size = 1, .stride = 1,
		.reg_read = sdam_read, .reg_write = sdam_write,
		.add_legacy_fixed_of_cells = true,
	};
	struct sdam *s = devm_kzalloc(&pdev->dev, sizeof(*s), GFP_KERNEL);
	int n;

	if (!s)
		return -ENOMEM;
	n = of_property_count_u8_elems(pdev->dev.of_node, "avr,preset");
	if (n > 0)
		of_property_read_u8_array(pdev->dev.of_node, "avr,preset", s->ram,
					  min(n, SDAM_SIZE));
	cfg.priv = s;
	s->nvmem = devm_nvmem_register(&pdev->dev, &cfg);
	if (IS_ERR(s->nvmem))
		return PTR_ERR(s->nvmem);
	if (of_property_read_bool(pdev->dev.of_node, "avr,restart-sdam"))
		restart_sdam = s;
	return 0;
}

static const struct of_device_id avr_sdam_match[] = { { .compatible = "avr,fake-sdam" }, {} };
static struct platform_driver avr_sdam_driver = {
	.probe = avr_sdam_probe,
	.driver = { .name = "avr-fake-sdam", .of_match_table = avr_sdam_match },
};

static int __init avr_sdam_init(void)
{
	int ret = platform_driver_register(&avr_sdam_driver);

	if (!ret)
		register_restart_handler(&restart_nb);
	return ret;
}
module_init(avr_sdam_init);
MODULE_LICENSE("GPL");
