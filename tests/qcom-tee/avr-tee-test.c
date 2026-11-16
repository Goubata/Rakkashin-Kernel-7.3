// SPDX-License-Identifier: GPL-2.0-only
/*
 * KUnit for the Stage109 TEE stack on QEMU: restored qtee_shmbridge/qcom_tzmem
 * (bridge registry, 6.12 bridge flag encoding, cached pools), the restored
 * vendor SMCInvoke SCM calls and the ported si_core transport, all running on
 * the real qcom-scm sources with tests/qcom-tee/avr_fake_tz.c in place of the
 * SMC instruction.
 */
#include <kunit/test.h>
#include <linux/delay.h>
#include <linux/firmware/qcom/qcom_scm.h>
#include <linux/firmware/qcom/qcom_tzmem.h>
#include <linux/firmware/qcom/si_object.h>
#include <linux/kthread.h>
#include <linux/module.h>
#include <linux/qtee_shmbridge.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include "avr_fake_tz.h"

#define SELF_OWNER_FLAGS ((1ULL << 1) | (6ULL << 2))	/* self-owner bit, RW */
#define NUM_VM_FLAG (1ULL << 9)

static void tee_bridge_registry(struct kunit *test)
{
	struct avr_fake_tz_stats a, b;
	void *pg = (void *)__get_free_pages(GFP_KERNEL, 1);
	phys_addr_t pa = virt_to_phys(pg);
	u32 vmid = 0x22, perm = 6;
	u64 h1 = 0, h2 = 0, h3 = 0;

	KUNIT_ASSERT_NOT_NULL(test, pg);
	KUNIT_ASSERT_TRUE(test, qtee_shmbridge_is_enabled());
	avr_fake_tz_get_stats(&a);

	/* first registration reaches TZ with the 1-VM encoding */
	KUNIT_ASSERT_EQ(test, qtee_shmbridge_register(pa, SZ_8K, &vmid, &perm, 1, 6, &h1), 0);
	avr_fake_tz_get_stats(&b);
	KUNIT_EXPECT_EQ(test, b.bridge_creates, a.bridge_creates + 1);
	KUNIT_EXPECT_EQ(test, b.last_bridge_flags, (u64)SZ_8K | NUM_VM_FLAG);
	KUNIT_EXPECT_EQ(test, b.last_bridge_vmids, 0x22ULL);
	KUNIT_EXPECT_EQ(test, b.last_bridge_pfn, (u64)pa | 6);
	KUNIT_EXPECT_EQ(test, qtee_shmbridge_query(pa), -EEXIST);

	/* re-registration returns the same handle, no new TZ bridge */
	KUNIT_ASSERT_EQ(test, qtee_shmbridge_register(pa, SZ_8K, &vmid, &perm, 1, 6, &h2), 0);
	KUNIT_EXPECT_EQ(test, h2, h1);
	avr_fake_tz_get_stats(&b);
	KUNIT_EXPECT_EQ(test, b.bridge_creates, a.bridge_creates + 1);

	/* first deregister only drops a reference */
	KUNIT_EXPECT_EQ(test, qtee_shmbridge_deregister(h1), 0);
	avr_fake_tz_get_stats(&b);
	KUNIT_EXPECT_EQ(test, b.bridge_deletes, a.bridge_deletes);
	KUNIT_EXPECT_EQ(test, qtee_shmbridge_query(pa), -EEXIST);
	KUNIT_EXPECT_EQ(test, qtee_shmbridge_deregister(h2), 0);
	avr_fake_tz_get_stats(&b);
	KUNIT_EXPECT_EQ(test, b.bridge_deletes, a.bridge_deletes + 1);
	KUNIT_EXPECT_EQ(test, b.bridges_live, a.bridges_live);
	KUNIT_EXPECT_EQ(test, qtee_shmbridge_query(pa), 0);

	/* default (vmid 0) bridge: 6.12 self-owner encoding */
	KUNIT_ASSERT_EQ(test, qcom_tzmem_shm_bridge_create(pa, SZ_4K + 1, &h3), 0);
	avr_fake_tz_get_stats(&b);
	KUNIT_EXPECT_EQ(test, b.last_bridge_flags, (u64)SZ_8K | SELF_OWNER_FLAGS);
	KUNIT_EXPECT_EQ(test, b.last_bridge_vmids, 0ULL);
	KUNIT_EXPECT_EQ(test, b.last_bridge_pfn, (u64)pa);
	qcom_tzmem_shm_bridge_delete(h3);
	avr_fake_tz_get_stats(&b);
	KUNIT_EXPECT_EQ(test, b.bridges_live, a.bridges_live);
	free_pages((unsigned long)pg, 1);
}

static void tee_shm_pool(struct kunit *test)
{
	struct qtee_shm *v = kunit_kcalloc(test, 40, sizeof(*v), GFP_KERNEL);
	struct avr_fake_tz_stats a, b;
	int n = 0, i, ret = 0;
	size_t j;

	avr_fake_tz_get_stats(&a);
	while (n < 40 && !(ret = qtee_shmbridge_allocate_shm(SZ_256K, &v[n])))
		n++;
	KUNIT_EXPECT_EQ(test, ret, -ENOMEM);
	KUNIT_EXPECT_GE(test, n, 24);
	KUNIT_EXPECT_LE(test, n, 32);
	avr_fake_tz_get_stats(&b);
	/* every pool growth is one bridged area */
	KUNIT_EXPECT_GT(test, b.bridge_creates, a.bridge_creates);
	for (i = 0; i < n; i++) {
		KUNIT_EXPECT_EQ(test, (u64)v[i].paddr, (u64)virt_to_phys(v[i].vaddr));
		for (j = 0; j < v[i].size; j += 4096)
			((u8 *)v[i].vaddr)[j] = i;
		qtee_shmbridge_flush_shm_buf(&v[i]);
	}
	for (i = 0; i < n; i++) {
		qtee_shmbridge_inv_shm_buf(&v[i]);
		KUNIT_EXPECT_EQ(test, ((u8 *)v[i].vaddr)[v[i].size - 4096], (u8)i);
	}
	while (n--)
		qtee_shmbridge_free_shm(&v[n]);
}

/* ---- si_core transport ---- */
static struct si_object_invoke_ctx oic;
static atomic_t cb_calls, cb_release;
static char cb_seen[32];

static int cb_dispatch(unsigned int ctx, struct si_object *o, unsigned long op,
		       struct si_arg args[])
{
	atomic_inc(&cb_calls);
	if (op != 7 || args[0].type != SI_AT_IB || args[1].type != SI_AT_OB)
		return -EINVAL;
	memcpy(cb_seen, args[0].b.addr, min_t(size_t, args[0].b.size, sizeof(cb_seen) - 1));
	snprintf(args[1].b.addr, args[1].b.size, "pong:%s", cb_seen);
	args[1].b.size = strlen(args[1].b.addr) + 1;
	return 0;
}

static void cb_release_fn(struct si_object *o)
{
	atomic_inc(&cb_release);
	free_si_object(o);
}

static struct si_object_operations cb_ops = {
	.dispatch = cb_dispatch,
	.release = cb_release_fn,
};

static int invoke(struct si_object *obj, unsigned long op, struct si_arg *a, int *res)
{
	return si_object_do_invoke(&oic, obj, op, a, res);
}

static struct si_object *open_svc(struct kunit *test, u32 uid, int *result)
{
	struct si_arg a[3] = {
		{ .type = SI_AT_IB, .b = { { &uid }, sizeof(uid) } },
		{ .type = SI_AT_OO },
		{ .type = SI_AT_END },
	};

	KUNIT_EXPECT_EQ(test, invoke(ROOT_SI_OBJECT, 0, a, result), 0);
	return *result ? NULL : a[1].o;
}

static void tee_si_core_invoke(struct kunit *test)
{
	char in[] = "hello qtee", out[32] = {};
	struct si_object *svc, *svc2, *env = NULL, *cb;
	struct avr_fake_tz_stats a, b;
	int result = -1, i;

	avr_fake_tz_get_stats(&a);
	svc = open_svc(test, 0x1234, &result);
	KUNIT_ASSERT_EQ(test, result, 0);
	KUNIT_ASSERT_NOT_NULL(test, svc);
	KUNIT_EXPECT_EQ(test, typeof_si_object(svc), SI_OT_USER);

	/* a refusing service reports its error code, no object */
	KUNIT_EXPECT_NULL(test, open_svc(test, 0xdead, &result));
	KUNIT_EXPECT_EQ(test, result, 10);

	/* echo: input buffer marshalled in, output buffer copied back */
	{
		struct si_arg a[3] = {
			{ .type = SI_AT_IB, .b = { { in }, sizeof(in) } },
			{ .type = SI_AT_OB, .b = { { out }, sizeof(out) } },
			{ .type = SI_AT_END },
		};

		KUNIT_EXPECT_EQ(test, invoke(svc, 0, a, &result), 0);
		KUNIT_EXPECT_EQ(test, result, 0);
		KUNIT_EXPECT_EQ(test, a[1].b.size, sizeof(in));
		KUNIT_EXPECT_STREQ(test, out, in);
	}

	/* QTEE calls back into a kernel object, then releases it */
	cb = allocate_si_object();
	KUNIT_ASSERT_NOT_NULL(test, cb);
	KUNIT_ASSERT_EQ(test, init_si_object_user(cb, SI_OT_CB_OBJECT, &cb_ops, "avr-cb"), 0);
	{
		char ping[] = "ping";
		struct si_arg a[4] = {
			{ .type = SI_AT_IB, .b = { { ping }, sizeof(ping) } },
			{ .type = SI_AT_OB, .b = { { out }, sizeof(out) } },
			{ .type = SI_AT_IO, .o = cb },
			{ .type = SI_AT_END },
		};

		memset(out, 0, sizeof(out));
		KUNIT_EXPECT_EQ(test, invoke(svc, 1, a, &result), 0);
		KUNIT_EXPECT_EQ(test, result, 0);
		KUNIT_EXPECT_STREQ(test, out, "pong:ping");
	}
	KUNIT_EXPECT_EQ(test, atomic_read(&cb_calls), 1);
	KUNIT_EXPECT_EQ(test, atomic_read(&cb_release), 1);
	avr_fake_tz_get_stats(&b);
	KUNIT_EXPECT_EQ(test, b.releases_from_ns, a.releases_from_ns + 1);
	KUNIT_EXPECT_EQ(test, b.ns_objects_held, a.ns_objects_held);

	/* output object from a QTEE object, then released through si_core's WQ */
	{
		struct si_arg a[2] = { { .type = SI_AT_OO }, { .type = SI_AT_END } };

		KUNIT_EXPECT_EQ(test, invoke(svc, 2, a, &result), 0);
		KUNIT_EXPECT_EQ(test, result, 0);
		svc2 = a[0].o;
		KUNIT_EXPECT_EQ(test, typeof_si_object(svc2), SI_OT_USER);
	}

	/* SCM busy is not replayed by the SCM layer; si_core backs off and retries */
	{
		struct si_arg a[1] = { { .type = SI_AT_END } };

		KUNIT_EXPECT_EQ(test, invoke(svc, 3, a, &result), 0);
		KUNIT_EXPECT_EQ(test, result, 0);
	}

	/* client env through registerWithCredentials, then open from it */
	KUNIT_EXPECT_EQ(test, si_core_get_client_env(&oic, &env), 0);
	KUNIT_ASSERT_NOT_NULL(test, env);
	{
		struct si_object *svc3 = NULL;

		KUNIT_EXPECT_EQ(test, si_core_client_env_open(&oic, env, 0x42, &svc3), 0);
		KUNIT_EXPECT_NOT_NULL(test, svc3);
		put_si_object(svc3);
	}

	put_si_object(svc2);
	put_si_object(env);
	put_si_object(svc);
	for (i = 0; i < 200; i++) {
		avr_fake_tz_get_stats(&b);
		if (b.services_live == a.services_live)
			break;
		msleep(10);
	}
	KUNIT_EXPECT_EQ(test, b.services_live, a.services_live);
}

static atomic_t storm_bad;
static int storm(void *arg)
{
	struct si_object_invoke_ctx *c = kzalloc(sizeof(*c), GFP_KERNEL);
	char in[24], out[24];
	int i, result;
	u32 uid = 0x77;

	if (!c) {
		atomic_inc(&storm_bad);
		return 0;
	}
	for (i = 0; i < 200; i++) {
		struct si_arg o[3] = {
			{ .type = SI_AT_IB, .b = { { &uid }, sizeof(uid) } },
			{ .type = SI_AT_OO }, { .type = SI_AT_END } };
		struct si_object *s;

		if (si_object_do_invoke(c, ROOT_SI_OBJECT, 0, o, &result) || result) {
			atomic_inc(&storm_bad);
			continue;
		}
		s = o[1].o;
		snprintf(in, sizeof(in), "t%ld-%d", (long)arg, i);
		{
			struct si_arg e[3] = {
				{ .type = SI_AT_IB, .b = { { in }, strlen(in) + 1 } },
				{ .type = SI_AT_OB, .b = { { out }, sizeof(out) } },
				{ .type = SI_AT_END } };

			if (si_object_do_invoke(c, s, 0, e, &result) || result || strcmp(in, out))
				atomic_inc(&storm_bad);
		}
		put_si_object(s);
	}
	kfree(c);
	return 0;
}

static void tee_si_core_concurrent(struct kunit *test)
{
	struct task_struct *t[4];
	struct avr_fake_tz_stats a, b;
	long i;

	avr_fake_tz_get_stats(&a);
	atomic_set(&storm_bad, 0);
	for (i = 0; i < 4; i++) {
		t[i] = kthread_run(storm, (void *)i, "tee-storm/%ld", i);
		KUNIT_ASSERT_FALSE(test, IS_ERR(t[i]));
		get_task_struct(t[i]);
	}
	for (i = 0; i < 4; i++) {
		kthread_stop(t[i]);
		put_task_struct(t[i]);
	}
	KUNIT_EXPECT_EQ(test, atomic_read(&storm_bad), 0);
	for (i = 0; i < 300; i++) {
		avr_fake_tz_get_stats(&b);
		if (b.services_live == a.services_live)
			break;
		msleep(10);
	}
	KUNIT_EXPECT_EQ(test, b.services_live, a.services_live);
}

static struct kunit_case cases[] = {
	KUNIT_CASE(tee_bridge_registry),
	KUNIT_CASE(tee_shm_pool),
	KUNIT_CASE(tee_si_core_invoke),
	KUNIT_CASE(tee_si_core_concurrent),
	{}
};

static struct kunit_suite suite = { .name = "avr_qcom_tee", .test_cases = cases };
kunit_test_suite(suite);
MODULE_LICENSE("GPL");
MODULE_SOFTDEP("pre: si_core_module");
