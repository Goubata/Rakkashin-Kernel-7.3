// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/pci.h>
#include <ufs/ufshcd.h>

static bool expected_mcq;
module_param(expected_mcq, bool, 0400);
struct context { struct pci_dev *pdev; struct ufs_hba *hba; };
static int init(struct kunit *test)
{
	struct context *ctx = kunit_kzalloc(test, sizeof(*ctx), GFP_KERNEL);

	if (!ctx)
		return -ENOMEM;
	ctx->pdev = pci_get_device(PCI_VENDOR_ID_REDHAT, 0x0013, NULL);
	if (!ctx->pdev)
		return -ENODEV;
	ctx->hba = pci_get_drvdata(ctx->pdev);
	if (!ctx->hba || ctx->hba->dev != &ctx->pdev->dev) {
		pci_dev_put(ctx->pdev);
		return -ENODEV;
	}
	test->priv = ctx;
	return 0;
}
static void exit_test(struct kunit *test)
{
	struct context *ctx = test->priv;

	pci_dev_put(ctx->pdev);
}
static void controller_active(struct kunit *test)
{
	struct context *ctx = test->priv;

	KUNIT_EXPECT_EQ(test, ctx->hba->ufshcd_state, UFSHCD_STATE_OPERATIONAL);
	KUNIT_EXPECT_TRUE(test, ctx->hba->scsi_host_added);
	KUNIT_EXPECT_NOT_NULL(test, ctx->hba->ufs_device_wlun);
}
static void real_irq_bookkeeping(struct kunit *test)
{
	struct context *ctx = test->priv;
	u64 stamp = READ_ONCE(ctx->hba->ufs_stats.last_intr_ts);

	KUNIT_EXPECT_GT(test, stamp, 0ULL);
	kunit_info(test, "real IRQ timestamp_ns=%llu status=0x%x\n", stamp,
		   READ_ONCE(ctx->hba->ufs_stats.last_intr_status));
}
static void queue_mode(struct kunit *test)
{
	struct context *ctx = test->priv;

	KUNIT_EXPECT_EQ(test, ctx->hba->mcq_enabled, expected_mcq);
	if (expected_mcq)
		KUNIT_EXPECT_GT(test, ctx->hba->nr_hw_queues, 0U);
	else
		KUNIT_EXPECT_GT(test, ctx->hba->nutrs, 0);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(controller_active), KUNIT_CASE(real_irq_bookkeeping),
	KUNIT_CASE(queue_mode), {}
};
static struct kunit_suite suite = {
	.name = "avr-ufs-core", .init = init, .exit = exit_test, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("QEMU UFS core enumeration, IRQ and queue checks after real I/O");
