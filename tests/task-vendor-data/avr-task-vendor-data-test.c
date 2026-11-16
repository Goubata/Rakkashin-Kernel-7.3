// SPDX-License-Identifier: GPL-2.0-only
/* Native fork/kthread/exit paths with the restored GKI dynamic task_struct
 * vendor area. Expected size comes from the guest command line. */
#include <kunit/test.h>
#include <linux/completion.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/sched/task.h>
#include <linux/cache.h>
#include <linux/slab.h>

static unsigned int expect_vendor_bytes = 512;
module_param(expect_vendor_bytes, uint, 0444);

/* arch_task_struct_size is not exported (same as 6.12); measure the slab object. */
static void size_from_cmdline(struct kunit *test)
{
	size_t obj = ksize(current), base = sizeof(struct task_struct);

	KUNIT_EXPECT_LE(test, expect_vendor_bytes, (unsigned int)CONFIG_GKI_TASK_STRUCT_VENDOR_SIZE_MAX);
	KUNIT_EXPECT_GE(test, obj, base + expect_vendor_bytes);
	/* Absent/rejected parameter must not add an area (allow cache-line rounding). */
	KUNIT_EXPECT_LT(test, obj, base + expect_vendor_bytes + L1_CACHE_BYTES);
}

static void init_task_uses_pad(struct kunit *test)
{
	KUNIT_EXPECT_PTR_EQ(test, android_task_vendor_data(&init_task), (void *)&vendor_data_pad[0]);
	KUNIT_EXPECT_PTR_EQ(test, android_task_vendor_data(current), (void *)(current + 1));
}

struct child_ctx {
	struct completion ready, release;
	u8 seen[CONFIG_GKI_TASK_STRUCT_VENDOR_SIZE_MAX];
	size_t len;
};

static int child_fn(void *data)
{
	struct child_ctx *c = data;

	memcpy(c->seen, android_task_vendor_data(current), c->len);
	/* Dirty our own area: must neither leak to the parent nor to later forks. */
	memset(android_task_vendor_data(current), 0xa5, c->len);
	complete(&c->ready);
	wait_for_completion(&c->release);
	return 0;
}

static void fork_zeroes_and_isolates(struct kunit *test)
{
	size_t len = expect_vendor_bytes;
	u8 *parent = android_task_vendor_data(current), *snap;
	struct child_ctx *c;
	struct task_struct *t;
	int round;

	if (!len)
		kunit_skip(test, "no vendor area configured on this boot");
	snap = kunit_kmalloc(test, len, GFP_KERNEL);
	c = kunit_kzalloc(test, sizeof(*c), GFP_KERNEL);
	KUNIT_ASSERT_NOT_NULL(test, snap);
	KUNIT_ASSERT_NOT_NULL(test, c);
	/* Parent pattern must not be copied into children by dup_task_struct. */
	memset(parent, 0x3c, len);
	memcpy(snap, parent, len);
	for (round = 0; round < 64; round++) {
		size_t i, nonzero = 0;

		init_completion(&c->ready);
		init_completion(&c->release);
		c->len = len;
		t = kthread_run(child_fn, c, "avr-vdata-%d", round);
		KUNIT_ASSERT_FALSE(test, IS_ERR(t));
		get_task_struct(t);
		wait_for_completion(&c->ready);
		for (i = 0; i < len; i++)
			nonzero += c->seen[i] != 0;
		KUNIT_EXPECT_EQ(test, nonzero, (size_t)0);
		complete(&c->release);
		kthread_stop(t);
		put_task_struct(t);
	}
	KUNIT_EXPECT_EQ(test, memcmp(parent, snap, len), 0);
	memset(parent, 0, len);
}

/* The slab object must actually cover the vendor area (no neighbour overlap). */
static void slab_object_covers_area(struct kunit *test)
{
	struct task_struct *t = current;

	KUNIT_EXPECT_GE(test, ksize(t), sizeof(struct task_struct) + expect_vendor_bytes);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(size_from_cmdline), KUNIT_CASE(init_task_uses_pad),
	KUNIT_CASE(fork_zeroes_and_isolates), KUNIT_CASE(slab_object_covers_area), {}
};
static struct kunit_suite suite = { .name = "avr-task-vendor-data", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
