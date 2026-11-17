// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU-only TrustZone stand-in, linked into a test build of the REAL qcom-scm
 * sources in place of the SMC instruction (prepare_fake_scm.py). It answers:
 *   - SIP INFO/IS_CALL_AVAIL (convention probe and feature checks)
 *   - SIP MP SHM bridge enable/create/delete (records flags/vmids per bridge)
 *   - TRUSTED_OS SMCInvoke INVOKE / CB_RSP with a miniature QTEE using the real
 *     wire format (qtee_object_invoke / qtee_callback, counts, 8-byte aligned
 *     buffer offsets, NS object ids with bit 31):
 *       root (1):   op 0 open(uid)->service, op 5 registerWithCredentials(cred)->env
 *                   (QTEE releases the credentials object back through a
 *                   callback), op 8 adciAccept -> OBJECT_ERROR_INVALID, op 9 OK
 *       env:        op 0 open
 *       service:    op 0 echo bi0->bo0; op 1 call NS object oi0 with op 7
 *                   (bi0 in, 64-byte bo out) via INBOUND_REQ_NEEDED, return its
 *                   output and result, then RELEASE the NS object; op 2 returns
 *                   a new service object; op 3 answers SCM busy twice;
 *                   op 0xFFFF release
 * Everything else returns QCOM_SCM_EOPNOTSUPP. Never shipped.
 */
#include <linux/arm-smccc.h>
#include <linux/export.h>
#include <linux/io.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include "qcom_scm.h"
#include "avr_fake_tz.h"

#define OWNER_SIP	ARM_SMCCC_OWNER_SIP
#define OWNER_TOS	ARM_SMCCC_OWNER_TRUSTED_OS
#define FN(svc, cmd)	(((svc) << 8) | (cmd))

/* ---- QTEE transport format (drivers/firmware/qcom/si_core/si_core.h) ---- */
union msg_arg { struct { u32 offset, size; } b; u32 o; };
struct oi_msg { u32 cxt, op, counts; union msg_arg args[]; };
struct cb_msg { u32 result, cxt, op, counts; union msg_arg args[]; };
#define NUM_BI(c)	(((c) >> 0) & 0xf)
#define NUM_BO(c)	(((c) >> 4) & 0xf)
#define NUM_OI(c)	(((c) >> 8) & 0xf)
#define NUM_OO(c)	(((c) >> 12) & 0xf)
#define IDX_BO(c)	(NUM_BI(c))
#define IDX_OI(c)	(IDX_BO(c) + NUM_BO(c))
#define IDX_OO(c)	(IDX_OI(c) + NUM_OI(c))
#define COUNTS(bi, bo, oi, oo) ((bi) | ((bo) << 4) | ((oi) << 8) | ((oo) << 12))
#define ALIGN8(x)	ALIGN((x), 8)
#define QTEE_OBJ_ROOT	1U
#define QTEE_NS_BIT	(1U << 31)
#define RESP_DONE	0
#define RESP_INBOUND	3	/* QTEE_RESULT_INBOUND_REQ_NEEDED */
#define OBJ_OK		0
#define OBJ_ERR_INVALID	2
#define OBJ_ERR_SIZE_OUT 4
#define OBJ_ERR_USER	10
#define OBJ_ERR_DEFUNCT	(-90)
#define OP_RELEASE	0xffff

enum kind { K_FREE, K_SERVICE, K_ENV };
#define MAX_OBJ 64
static enum kind objs[MAX_OBJ];		/* QTEE object ids 0x100 + index */
#define OBJ_BASE 0x100

/* continuation of an invocation suspended on a callback (keyed by out buffer) */
struct cont {
	struct list_head node;
	phys_addr_t out;
	int stage;		/* 1: waiting op-7 reply, 2: waiting release reply */
	u32 ns_obj;
	struct oi_msg *in;	/* original inbound message (outputs go here) */
	int result;
};

static DEFINE_SPINLOCK(tz_lock);
static LIST_HEAD(conts);
static struct avr_fake_tz_stats st;
static u64 next_bridge = 0x1000;
struct bridge { struct list_head node; u64 handle; };
static LIST_HEAD(bridges);
#define NIO 8
static struct { u64 addr; u32 val; } io[NIO];

static u32 *io_slot(u64 addr)
{
	int i;

	for (i = 0; i < NIO; i++)
		if (io[i].addr == addr)
			return &io[i].val;
	for (i = 0; i < NIO; i++)
		if (!io[i].addr) {
			io[i].addr = addr;
			return &io[i].val;
		}
	return NULL;
}

static int busy_left = 2;	/* op 3: two SCM busy answers, then success */

void avr_fake_tz_get_stats(struct avr_fake_tz_stats *s)
{
	unsigned long f;

	spin_lock_irqsave(&tz_lock, f);
	*s = st;
	spin_unlock_irqrestore(&tz_lock, f);
}
EXPORT_SYMBOL_GPL(avr_fake_tz_get_stats);

static void *buf(void *msg, union msg_arg *a)
{
	return (char *)msg + a->b.offset;
}

static u32 new_obj(enum kind k)
{
	int i;

	for (i = 0; i < MAX_OBJ; i++)
		if (objs[i] == K_FREE) {
			objs[i] = k;
			st.services_live++;
			return OBJ_BASE + i;
		}
	return 0;
}

static enum kind kind_of(u32 id)
{
	if (id < OBJ_BASE || id >= OBJ_BASE + MAX_OBJ)
		return K_FREE;
	return objs[id - OBJ_BASE];
}

/* write a callback request for NS object @ns into @out; returns RESP_INBOUND */
static void make_cb(struct cb_msg *cb, size_t out_size, u32 ns, u32 op,
		    const void *in, u32 in_len, u32 out_len)
{
	u32 nargs = (in ? 1 : 0) + (out_len ? 1 : 0), off;

	memset(cb, 0, min_t(size_t, out_size, 256));
	cb->cxt = ns;
	cb->op = op;
	cb->counts = COUNTS(in ? 1 : 0, out_len ? 1 : 0, 0, 0);
	off = ALIGN8(offsetof(struct cb_msg, args) + nargs * sizeof(union msg_arg));
	if (in) {
		cb->args[0].b.offset = off;
		cb->args[0].b.size = in_len;
		memcpy((char *)cb + off, in, in_len);
		off = ALIGN8(off + in_len);
	}
	if (out_len) {
		cb->args[nargs - 1].b.offset = off;
		cb->args[nargs - 1].b.size = out_len;
	}
	st.callbacks++;
}

static void open_service(struct oi_msg *m, u32 c, int *result)
{
	u32 uid;

	if (NUM_BI(c) < 1 || NUM_OO(c) < 1 || m->args[0].b.size < 4) {
		*result = OBJ_ERR_INVALID;
		return;
	}
	memcpy(&uid, buf(m, &m->args[0]), 4);
	if (uid == 0xdead) {
		*result = OBJ_ERR_USER;		/* a service that refuses to open */
		return;
	}
	m->args[IDX_OO(c)].o = new_obj(K_SERVICE);
	*result = m->args[IDX_OO(c)].o ? OBJ_OK : OBJ_ERR_INVALID;
}

/* returns response type; fills *result; may park a continuation */
static int qtee_invoke(phys_addr_t in_pa, size_t in_sz, phys_addr_t out_pa,
		       size_t out_sz, int *result, bool *busy)
{
	struct oi_msg *m;
	struct cb_msg *cb;
	u32 c, op;
	enum kind k;
	struct cont *ct;
	u32 i;

	st.invokes++;
	/* like QTEE: a call without valid buffers is answered, not executed
	 * (7.3 qcom_scm_qtee_init probes smcinvoke support this way) */
	if (!in_pa || !out_pa || in_sz < sizeof(*m) || out_sz < 256) {
		*result = OBJ_ERR_INVALID;
		return RESP_DONE;
	}
	m = phys_to_virt(in_pa);
	cb = phys_to_virt(out_pa);
	c = m->counts;
	op = m->op & 0xffff;
	k = kind_of(m->cxt);
	*result = OBJ_OK;
	for (i = 0; i < NUM_OI(c); i++)
		if (m->args[IDX_OI(c) + i].o & QTEE_NS_BIT)
			st.ns_objects_held++;	/* QTEE now owns a reference */

	if (op == OP_RELEASE && (k == K_SERVICE || k == K_ENV)) {
		objs[m->cxt - OBJ_BASE] = K_FREE;
		st.services_live--;
		return RESP_DONE;
	}

	if (m->cxt == QTEE_OBJ_ROOT || k == K_ENV) {
		switch (op) {
		case 0:
			open_service(m, c, result);
			return RESP_DONE;
		case 5:
			if (m->cxt != QTEE_OBJ_ROOT || NUM_OO(c) < 1) {
				*result = OBJ_ERR_INVALID;
				return RESP_DONE;
			}
			m->args[IDX_OO(c)].o = new_obj(K_ENV);
			if (NUM_OI(c) && (m->args[IDX_OI(c)].o & QTEE_NS_BIT)) {
				/* QTEE drops the credentials object when done with it */
				ct = kzalloc(sizeof(*ct), GFP_ATOMIC);
				if (!ct) {
					*result = OBJ_ERR_INVALID;
					return RESP_DONE;
				}
				ct->out = out_pa;
				ct->stage = 2;
				ct->ns_obj = m->args[IDX_OI(c)].o;
				ct->in = m;
				list_add(&ct->node, &conts);
				make_cb(cb, out_sz, ct->ns_obj, OP_RELEASE, NULL, 0, 0);
				return RESP_INBOUND;
			}
			return RESP_DONE;
		case 8:
			*result = OBJ_ERR_INVALID;	/* ADCI not offered */
			return RESP_DONE;
		case 9:
			return RESP_DONE;
		}
		*result = OBJ_ERR_INVALID;
		return RESP_DONE;
	}

	if (k != K_SERVICE) {
		*result = OBJ_ERR_DEFUNCT;
		return RESP_DONE;
	}

	switch (op) {
	case 0: {	/* echo */
		union msg_arg *bi = &m->args[0], *bo = &m->args[IDX_BO(c)];

		if (NUM_BI(c) < 1 || NUM_BO(c) < 1) {
			*result = OBJ_ERR_INVALID;
			break;
		}
		if (bo->b.size < bi->b.size) {
			*result = OBJ_ERR_SIZE_OUT;
			break;
		}
		memmove(buf(m, bo), buf(m, bi), bi->b.size);
		bo->b.size = bi->b.size;
		break;
	}
	case 1: {	/* call back into the normal world */
		u32 ns;

		if (NUM_BI(c) < 1 || NUM_BO(c) < 1 || NUM_OI(c) < 1) {
			*result = OBJ_ERR_INVALID;
			break;
		}
		ns = m->args[IDX_OI(c)].o;
		ct = kzalloc(sizeof(*ct), GFP_ATOMIC);
		if (!ct) {
			*result = OBJ_ERR_INVALID;
			break;
		}
		ct->out = out_pa;
		ct->stage = 1;
		ct->ns_obj = ns;
		ct->in = m;
		list_add(&ct->node, &conts);
		make_cb(cb, out_sz, ns, 7, buf(m, &m->args[0]), m->args[0].b.size, 64);
		return RESP_INBOUND;
	}
	case 2:
		if (NUM_OO(c) < 1) {
			*result = OBJ_ERR_INVALID;
			break;
		}
		m->args[IDX_OO(c)].o = new_obj(K_SERVICE);
		break;
	case 3:
		if (busy_left > 0) {
			busy_left--;
			*busy = true;
			return RESP_DONE;
		}
		busy_left = 2;
		break;
	case OP_RELEASE:
		objs[m->cxt - OBJ_BASE] = K_FREE;
		st.services_live--;
		break;
	default:
		*result = OBJ_ERR_INVALID;
	}
	return RESP_DONE;
}

static int qtee_cb_response(phys_addr_t out_pa, size_t out_sz, int *result)
{
	struct cb_msg *cb = phys_to_virt(out_pa);
	struct cont *ct = NULL, *t;
	struct oi_msg *m;
	u32 c;

	list_for_each_entry(t, &conts, node)
		if (t->out == out_pa) {
			ct = t;
			break;
		}
	if (!ct) {
		*result = OBJ_ERR_INVALID;
		return RESP_DONE;
	}
	m = ct->in;
	c = m->counts;
	if (ct->stage == 1) {
		union msg_arg *bo = &m->args[IDX_BO(c)];
		union msg_arg *cbo = &cb->args[1];	/* bi, bo */

		ct->result = cb->result;
		if (!cb->result) {
			u32 n = min(cbo->b.size, bo->b.size);

			memcpy(buf(m, bo), (char *)cb + cbo->b.offset, n);
			bo->b.size = n;
		}
		ct->stage = 2;
		make_cb(cb, out_sz, ct->ns_obj, OP_RELEASE, NULL, 0, 0);
		return RESP_INBOUND;
	}
	/* stage 2: the NS object release was acknowledged */
	if (!cb->result) {
		st.releases_from_ns++;
		st.ns_objects_held--;
	}
	*result = ct->result;
	list_del(&ct->node);
	kfree(ct);
	return RESP_DONE;
}

static void fake_call(unsigned long a0, unsigned long a1, unsigned long a2,
		      unsigned long a3, unsigned long a4, unsigned long a5,
		      struct arm_smccc_res *res)
{
	u32 owner = ARM_SMCCC_OWNER_NUM(a0), fn = ARM_SMCCC_FUNC_NUM(a0);
	unsigned long f;
	int result = 0, resp;
	bool busy = false;

	memset(res, 0, sizeof(*res));
	spin_lock_irqsave(&tz_lock, f);
	if (owner == OWNER_SIP && fn == FN(QCOM_SCM_SVC_INFO, QCOM_SCM_INFO_IS_CALL_AVAIL)) {
		u32 q = a2 & 0xffff;

		res->a1 = q == FN(QCOM_SCM_SVC_INFO, QCOM_SCM_INFO_IS_CALL_AVAIL) ||
			  q == FN(QCOM_SCM_SVC_MP, QCOM_SCM_MP_SHM_BRIDGE_ENABLE) ||
			  q == FN(QCOM_SCM_SVC_MP, QCOM_SCM_MP_SHM_BRIDGE_CREATE) ||
			  q == FN(QCOM_SCM_SVC_MP, QCOM_SCM_MP_SHM_BRIDGE_DELETE);
	} else if (owner == OWNER_SIP && fn == FN(QCOM_SCM_SVC_MP, QCOM_SCM_MP_SHM_BRIDGE_ENABLE)) {
		res->a1 = 0;
	} else if (owner == OWNER_SIP && fn == FN(QCOM_SCM_SVC_MP, QCOM_SCM_MP_SHM_BRIDGE_CREATE)) {
		struct bridge *b = kzalloc(sizeof(*b), GFP_ATOMIC);

		if (!b) {
			res->a0 = QCOM_SCM_ENOMEM;
		} else {
			b->handle = next_bridge++;
			list_add(&b->node, &bridges);
			st.bridges_live++;
			st.bridge_creates++;
			st.last_bridge_pfn = a2;
			st.last_bridge_flags = a4;
			st.last_bridge_vmids = a5;
			res->a2 = b->handle;
		}
	} else if (owner == OWNER_SIP && fn == FN(QCOM_SCM_SVC_MP, QCOM_SCM_MP_SHM_BRIDGE_DELETE)) {
		struct bridge *b;

		res->a0 = QCOM_SCM_EINVAL_ARG;
		list_for_each_entry(b, &bridges, node)
			if (b->handle == a2) {
				list_del(&b->node);
				kfree(b);
				st.bridges_live--;
				st.bridge_deletes++;
				res->a0 = 0;
				break;
			}
	} else if (owner == OWNER_TOS && fn == FN(QCOM_SCM_SVC_SMCINVOKE, QCOM_SCM_SMCINVOKE_INVOKE)) {
		resp = qtee_invoke(a2, a3, a4, a5, &result, &busy);
		if (busy) {
			res->a0 = QCOM_SCM_V2_EBUSY;
		} else {
			res->a1 = resp;
			res->a2 = (u32)result;
		}
	} else if (owner == OWNER_TOS && fn == FN(QCOM_SCM_SVC_SMCINVOKE, QCOM_SCM_SMCINVOKE_CB_RSP)) {
		resp = qtee_cb_response(a2, a3, &result);
		res->a1 = resp;
		res->a2 = (u32)result;
	} else if (owner == OWNER_SIP && fn == FN(QCOM_SCM_SVC_IO, QCOM_SCM_IO_READ)) {
		u32 *v = io_slot(a2);

		res->a1 = v ? *v : 0;
	} else if (owner == OWNER_SIP && fn == FN(QCOM_SCM_SVC_IO, QCOM_SCM_IO_WRITE)) {
		u32 *v = io_slot(a2);

		if (v)
			*v = a3;
		st.io_writes++;
		st.last_io_addr = a2;
		st.last_io_val = a3;
	} else if (owner == OWNER_SIP && fn == FN(QCOM_SCM_SVC_BOOT, QCOM_SCM_BOOT_SDI_CONFIG)) {
		st.sdi_disables += (a2 == 1 && a3 == 0);
	} else if (owner == OWNER_SIP && fn == FN(0x03, 0x13)) {
		st.dump_assigns += a2 == 1;
		st.dump_addr = a3;
		st.dump_size = a4;
		if (a2 == 1 && a3 && pfn_valid(PHYS_PFN(a3))) {
			/* read the table like the boot loader's ramdump would:
			 * {u32 version, u32 num_entries, {u32 id; char name[32];
			 *  u32 type; u64 addr} entries[]} */
			u32 *top = phys_to_virt(a3);
			u64 apps = *(u64 *)((u8 *)top + 8 + 40);

			st.dump_top_ver = top[0];
			st.dump_top_n = top[1];
			if (top[1] && pfn_valid(PHYS_PFN(apps))) {
				u32 *t = phys_to_virt(apps);

				st.dump_apps_ver = t[0];
				st.dump_apps_n = t[1];
			}
		}
	} else if (owner == ARM_SMCCC_OWNER_OEM && fn == FN(0x09, 0x22)) {
		st.oem_reboots++;
	} else {
		st.unknown_calls++;
		res->a0 = QCOM_SCM_EOPNOTSUPP;
	}
	spin_unlock_irqrestore(&tz_lock, f);
}

void avr_fake_smc_quirk(unsigned long a0, unsigned long a1, unsigned long a2,
			unsigned long a3, unsigned long a4, unsigned long a5,
			unsigned long a6, unsigned long a7, struct arm_smccc_res *res,
			struct arm_smccc_quirk *quirk)
{
	fake_call(a0, a1, a2, a3, a4, a5, res);
	if (quirk)
		quirk->state.a6 = 0;
}

void avr_fake_smc(unsigned long a0, unsigned long a1, unsigned long a2,
		  unsigned long a3, unsigned long a4, unsigned long a5,
		  unsigned long a6, unsigned long a7, struct arm_smccc_res *res)
{
	memset(res, 0, sizeof(*res));
	res->a0 = QCOM_SCM_EOPNOTSUPP;	/* legacy convention is never offered */
}

/* shell-visible state: /sys/module/qcom_scm/parameters/avr_state */
static int avr_state_get(char *buf, const struct kernel_param *kp)
{
	struct avr_fake_tz_stats s;

	avr_fake_tz_get_stats(&s);
	return sysfs_emit(buf, "io=0x%llx:0x%x writes=%u sdi=%u oem=%u dump=%u:0x%llx:0x%llx table=%x/%u apps=%x/%u\n",
			  s.last_io_addr, s.last_io_val, s.io_writes, s.sdi_disables,
			  s.oem_reboots, s.dump_assigns, s.dump_addr, s.dump_size,
			  s.dump_top_ver, s.dump_top_n, s.dump_apps_ver, s.dump_apps_n);
}
static const struct kernel_param_ops avr_state_ops = { .get = avr_state_get };
module_param_cb(avr_state, &avr_state_ops, NULL, 0444);
