// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/ipc_logging.h>
#include <soc/qcom/minidump.h>

extern bool md_init_done;
extern unsigned int md_num_regions;
static u32 test_data;

static struct md_region region(const char *name)
{
	struct md_region entry = {
		.virt_addr = (unsigned long)&test_data,
		.phys_addr = virt_to_phys(&test_data), .size = sizeof(test_data),
	};
	strscpy(entry.name, name, sizeof(entry.name));
	return entry;
}
/* These are intentionally pre-probe lifecycle tests in an isolated guest. */
static int pre_probe_only(struct kunit *test)
{
	return READ_ONCE(md_init_done) || msm_minidump_enabled() ? -EBUSY : 0;
}
static void pending_lifetime(struct kunit *test)
{
	struct md_region entry = region("avr_life");
	unsigned int baseline = READ_ONCE(md_num_regions);
	KUNIT_ASSERT_GE(test, msm_minidump_add_region(&entry), 0);
	KUNIT_EXPECT_EQ(test, READ_ONCE(md_num_regions), baseline + 1);
	KUNIT_EXPECT_EQ(test, msm_minidump_remove_region(&entry), 0);
	KUNIT_EXPECT_EQ(test, READ_ONCE(md_num_regions), baseline);
	KUNIT_EXPECT_EQ(test, msm_minidump_remove_region(&entry), -ENOENT);
}
static void duplicate_pending(struct kunit *test)
{
	struct md_region entry = region("avr_dup");
	unsigned int baseline = READ_ONCE(md_num_regions);
	KUNIT_ASSERT_GE(test, msm_minidump_add_region(&entry), 0);
	KUNIT_EXPECT_EQ(test, msm_minidump_add_region(&entry), -EEXIST);
	KUNIT_EXPECT_EQ(test, READ_ONCE(md_num_regions), baseline + 1);
	KUNIT_EXPECT_EQ(test, msm_minidump_remove_region(&entry), 0);
}
static void bounded_names(struct kunit *test)
{
	struct md_region entry = region("avr_name");
	memset(entry.name, 'x', sizeof(entry.name));
	KUNIT_EXPECT_EQ(test, msm_minidump_add_region(&entry), -EINVAL);
	KUNIT_EXPECT_EQ(test, msm_minidump_remove_region(&entry), -EINVAL);
	entry.name[0] = 0;
	KUNIT_EXPECT_EQ(test, msm_minidump_add_region(&entry), -EINVAL);
}
static void invalid_range(struct kunit *test)
{
	struct md_region entry = region("avr_invalid");
	KUNIT_EXPECT_EQ(test, msm_minidump_add_region(NULL), -EINVAL);
	KUNIT_EXPECT_EQ(test, msm_minidump_update_region(-1, &entry), -EINVAL);
	entry.size = 0;
	KUNIT_EXPECT_EQ(test, msm_minidump_add_region(&entry), -EINVAL);
	entry.size = 3;
	KUNIT_EXPECT_EQ(test, msm_minidump_add_region(&entry), -EINVAL);
	entry.size = 4; entry.virt_addr = 0;
	KUNIT_EXPECT_EQ(test, msm_minidump_add_region(&entry), -EINVAL);
}
static void quota_reuse(struct kunit *test)
{
	struct md_region *entries;
	unsigned int baseline = READ_ONCE(md_num_regions);
	int i, count = 0, ret;
	entries = kunit_kcalloc(test, 256, sizeof(*entries), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, entries);
	for (i = 0; i < 256; i++) {
		entries[i] = region("avr_quota");
		scnprintf(entries[i].name, sizeof(entries[i].name), "avr_q%03d", i);
		ret = msm_minidump_add_region(&entries[i]);
		if (ret < 0) {
			KUNIT_EXPECT_EQ(test, ret, -ENOMEM);
			break;
		}
		count++;
	}
	KUNIT_EXPECT_EQ(test, count, 200 - (int)baseline);
	KUNIT_EXPECT_LT(test, count, 256);
	if (count) {
		KUNIT_EXPECT_EQ(test, msm_minidump_remove_region(&entries[0]), 0);
		KUNIT_EXPECT_GE(test, msm_minidump_add_region(&entries[0]), 0);
	}
	for (i = 0; i < count; i++)
		KUNIT_EXPECT_EQ(test, msm_minidump_remove_region(&entries[i]), 0);
	KUNIT_EXPECT_EQ(test, READ_ONCE(md_num_regions), baseline);
}
static void ipc_roundtrip(struct kunit *test)
{
	char *buffer = kunit_kzalloc(test, 4096, GFP_KERNEL);
	void *context;
	int ret;
	KUNIT_ASSERT_NOT_NULL(test, buffer);
	context = ipc_log_context_create(2, "avr_test_roundtrip", 0x10000);
	KUNIT_ASSERT_NOT_NULL(test, context);
	KUNIT_EXPECT_EQ(test, ipc_log_string(context, "avr-runtime-%u", 7316), 0);
	ret = ipc_log_extract(context, buffer, 4095);
	KUNIT_EXPECT_GT(test, ret, 0);
	KUNIT_EXPECT_NOT_NULL(test, strnstr(buffer, "avr-runtime-7316", 4096));
	KUNIT_EXPECT_EQ(test, ipc_log_context_destroy(context), 0);
}
static void ipc_invalid(struct kunit *test)
{
	KUNIT_EXPECT_PTR_EQ(test, ipc_log_context_create(0, "zero", 0), NULL);
	KUNIT_EXPECT_PTR_EQ(test, ipc_log_context_create(-1, "neg", 0), NULL);
	KUNIT_EXPECT_PTR_EQ(test, ipc_log_context_create(1, NULL, 0), NULL);
	KUNIT_EXPECT_PTR_EQ(test, ipc_log_context_create(1, "", 0), NULL);
}
static void ipc_recreate(struct kunit *test)
{
	unsigned int baseline = READ_ONCE(md_num_regions);
	int i;
	for (i = 0; i < 32; i++) {
		void *context = ipc_log_context_create(2, "avr_recreate", 0x10000);
		KUNIT_ASSERT_NOT_NULL(test, context);
		KUNIT_EXPECT_EQ(test, ipc_log_string(context, "generation-%d", i), 0);
		KUNIT_EXPECT_EQ(test, ipc_log_context_destroy(context), 0);
	}
	KUNIT_EXPECT_EQ(test, READ_ONCE(md_num_regions), baseline);
}
static struct kunit_case cases[] = {
	KUNIT_CASE(pending_lifetime), KUNIT_CASE(duplicate_pending),
	KUNIT_CASE(bounded_names), KUNIT_CASE(invalid_range),
	KUNIT_CASE(quota_reuse), KUNIT_CASE(ipc_roundtrip),
	KUNIT_CASE(ipc_invalid), KUNIT_CASE(ipc_recreate), {}
};
static struct kunit_suite suite = {
	.name = "avr-diagnostics", .init = pre_probe_only, .test_cases = cases,
};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
