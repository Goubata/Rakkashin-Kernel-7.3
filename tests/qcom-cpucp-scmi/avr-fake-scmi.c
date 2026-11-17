// SPDX-License-Identifier: GPL-2.0-only
/*
 * QEMU-only stand-in for the canoe CPUCP/PDP SCMI firmware behind a mailbox.
 * Registers a mailbox controller ("avr,fake-scmi-platform", #mbox-cells = 1, like
 * qcom,cpucp-v2) and answers SCMI commands that the real 7.3 SCMI mailbox
 * transport writes into the arm,scmi-shmem areas named by "shmem":
 *   base 0x10 (v2.0): vendor "Qualcomm", sub-vendor from "avr,sub-vendor",
 *                     protocol list from "avr,protocols"
 *   perf 0x13 (v3.0, power in uW): the phone's two DVFS domains (policy0: 24 levels,
 *                     policy6: 21 levels + 4 above sustained), 30 us latency,
 *                     LEVEL/LIMITS set/get recorded
 *   qcom vendor 0x80 (v1.0): SET/GET_PARAM store/return a parameter blob,
 *                     START/STOP_ACTIVITY counted
 * Never shipped; the phone runs qcom_cpucp + CPUCP/PDP firmware.
 */
#include <linux/io.h>
#include <linux/mailbox_controller.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/unaligned.h>
#include <linux/workqueue.h>

#define NCHAN 8
struct shmem { __le32 reserved, status, reserved1[2], flags, length, hdr; u8 payload[]; };

struct fake {
	struct mbox_controller mbox;
	struct mbox_chan chans[NCHAN];
	struct shmem __iomem *tx;
	struct work_struct work;
	struct mbox_chan *pending;
	const char *sub_vendor;
	u32 protocols[4];
	int nprot;
};

static const u32 dom0[] = { 384000, 556800, 710400, 806400, 902400, 1036800, 1152000,
	1267200, 1401600, 1555200, 1651200, 1747200, 1900800, 2016000, 2188800, 2265600,
	2361600, 2476800, 2611200, 2707200, 2841600, 3033600, 3129600, 3321600 };
static const u32 dom1[] = { 384000, 556800, 806400, 921600, 1017600, 1152000, 1248000,
	1382400, 1497600, 1651200, 1862400, 1996800, 2112000, 2227200, 2476800, 2688000,
	2841600, 2995200, 3091200, 3244800, 3436800, 3532800, 3648000, 3724800, 3801600 };
static const struct { const u32 *lv; int n; u32 sustained; const char *name; } doms[] = {
	{ dom0, ARRAY_SIZE(dom0), 3321600, "silver" },
	{ dom1, ARRAY_SIZE(dom1), 3436800, "gold" },
};

/* test-visible state */
static u32 cur_level[2], lim_max[2], lim_min[2];
static unsigned int level_sets, vendor_sets, vendor_gets, activities;
static u8 param_blob[64];
static u32 param_len;

static int st_get(char *buf, const struct kernel_param *kp)
{
	return sysfs_emit(buf, "level=%u,%u sets=%u limits=%u-%u,%u-%u vendor_set=%u vendor_get=%u act=%u\n",
			  cur_level[0], cur_level[1], level_sets, lim_min[0], lim_max[0],
			  lim_min[1], lim_max[1], vendor_sets, vendor_gets, activities);
}
static const struct kernel_param_ops st_ops = { .get = st_get };
module_param_cb(state, &st_ops, NULL, 0444);

#define ST_OK 0
#define ST_NOT_SUPPORTED (-1)
#define ST_INVALID (-2)
#define ST_NOT_FOUND (-4)

static int reply_u32(u8 *out, const u32 *v, int n)
{
	int i;

	for (i = 0; i < n; i++)
		put_unaligned_le32(v[i], out + 4 * i);
	return 4 * n;
}

/* returns payload length after the status word, sets *st */
static int handle(struct fake *f, u32 prot, u32 id, const u8 *in, u32 in_len, u8 *out, int *st)
{
	u32 v[8];
	int i;

	*st = ST_OK;
	if (id == 0) {		/* PROTOCOL_VERSION */
		v[0] = prot == 0x80 ? 0x10000 : prot == 0x13 ? 0x30000 : 0x20000;
		return reply_u32(out, v, 1);
	}
	if (id == 2) {		/* PROTOCOL_MESSAGE_ATTRIBUTES */
		u32 m = get_unaligned_le32(in);

		if (prot == 0x13 && (m == 9 || m == 0xa || m >= 0xb)) {	/* no notify/fastchannel/ext names */
			*st = ST_NOT_FOUND;
			return 0;
		}
		v[0] = 0;
		return reply_u32(out, v, 1);
	}
	switch (prot) {
	case 0x10:
		switch (id) {
		case 1:
			v[0] = f->nprot | (1 << 8);
			return reply_u32(out, v, 1);
		case 3:
		case 4:
			memset(out, 0, 16);
			strscpy(out, id == 3 ? "Qualcomm" : f->sub_vendor, 16);
			return 16;
		case 5:
			v[0] = 0x00010000;
			return reply_u32(out, v, 1);
		case 6: {
			u32 skip = get_unaligned_le32(in), n = 0;

			memset(out, 0, 8);
			for (i = skip; i < f->nprot; i++)
				out[4 + n++] = f->protocols[i];
			put_unaligned_le32(n, out);
			return 4 + ALIGN(n, 4);
		}
		}
		break;
	case 0x13: {
		u32 d = in_len >= 4 ? get_unaligned_le32(in) : 0;

		if (id != 1 && d > 1) {
			*st = ST_NOT_FOUND;
			return 0;
		}
		switch (id) {
		case 1:
			v[0] = 2 | (BIT(1) << 16);	/* num_domains, power in uW */
			v[1] = v[2] = v[3] = 0;
			return reply_u32(out, v, 4);
		case 3:
			v[0] = BIT(31) | BIT(30);	/* set limits, set level */
			v[1] = 0;
			v[2] = doms[d].sustained;
			v[3] = doms[d].sustained;
			reply_u32(out, v, 4);
			memset(out + 16, 0, 16);
			strscpy(out + 16, doms[d].name, 16);
			return 32;
		case 4: {	/* DESCRIBE_LEVELS: 8 per reply */
			u32 idx = get_unaligned_le32(in + 4), n, rem;

			if (idx >= doms[d].n) {
				*st = ST_INVALID;
				return 0;
			}
			n = min_t(u32, 8, doms[d].n - idx);
			rem = doms[d].n - idx - n;
			put_unaligned_le16(n, out);
			put_unaligned_le16(rem, out + 2);
			for (i = 0; i < n; i++) {
				u8 *o = out + 4 + 12 * i;

				put_unaligned_le32(doms[d].lv[idx + i], o);
				put_unaligned_le32(doms[d].lv[idx + i], o + 4);	/* uW */
				put_unaligned_le16(30, o + 8);
				put_unaligned_le16(0, o + 10);
			}
			return 4 + 12 * n;
		}
		case 5:
			lim_max[d] = get_unaligned_le32(in + 4);
			lim_min[d] = get_unaligned_le32(in + 8);
			return 0;
		case 6:
			v[0] = lim_max[d] ? lim_max[d] : doms[d].lv[doms[d].n - 1];
			v[1] = lim_min[d] ? lim_min[d] : doms[d].lv[0];
			return reply_u32(out, v, 2);
		case 7:
			cur_level[d] = get_unaligned_le32(in + 4);
			level_sets++;
			return 0;
		case 8:
			v[0] = cur_level[d] ? cur_level[d] : doms[d].lv[0];
			return reply_u32(out, v, 1);
		}
		break;
	}
	case 0x80:
		switch (id) {
		case 1:
			v[0] = 0;
			return reply_u32(out, v, 1);
		case 0x10:	/* SET_PARAM: ext id, algo lo, algo hi, param id, data */
			param_len = min_t(u32, in_len > 16 ? in_len - 16 : 0, sizeof(param_blob));
			memcpy(param_blob, in + 16, param_len);
			vendor_sets++;
			return 0;
		case 0x11:
			vendor_gets++;
			memcpy(out, param_blob, param_len);
			return param_len;
		case 0x12:
		case 0x13:
			activities++;
			return 0;
		}
		break;
	}
	*st = ST_NOT_SUPPORTED;
	return 0;
}

static void fake_work(struct work_struct *w)
{
	struct fake *f = container_of(w, struct fake, work);
	u8 in[128], out[256];
	u32 hdr = ioread32(&f->tx->hdr), len = ioread32(&f->tx->length);
	u32 in_len = len > 4 ? min_t(u32, len - 4, sizeof(in)) : 0;
	int st, n;

	memcpy_fromio(in, f->tx->payload, in_len);
	n = handle(f, (hdr >> 10) & 0xff, hdr & 0xff, in, in_len, out + 4, &st);
	put_unaligned_le32((u32)st, out);
	memcpy_toio(f->tx->payload, out, 4 + n);
	iowrite32(4 + 4 + n, &f->tx->length);
	iowrite32(1, &f->tx->status);	/* channel free */
	mbox_chan_received_data(f->pending, NULL);
}

static int fake_send(struct mbox_chan *chan, void *data)
{
	struct fake *f = container_of(chan->mbox, struct fake, mbox);

	if (chan != &f->chans[0])
		return 0;	/* doorbell-only channels (rx, cpucp_fast) */
	f->pending = chan;
	queue_work(system_highpri_wq, &f->work);
	return 0;
}

static const struct mbox_chan_ops fake_ops = { .send_data = fake_send };

static int fake_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node, *shm;
	struct resource r;
	struct fake *f;

	f = devm_kzalloc(&pdev->dev, sizeof(*f), GFP_KERNEL);
	if (!f)
		return -ENOMEM;
	shm = of_parse_phandle(np, "shmem", 0);
	if (!shm || of_address_to_resource(shm, 0, &r))
		return -EINVAL;
	of_node_put(shm);
	f->tx = devm_ioremap(&pdev->dev, r.start, resource_size(&r));
	if (!f->tx)
		return -ENOMEM;
	iowrite32(1, &f->tx->status);	/* firmware marks the a2p channel free */
	of_property_read_string(np, "avr,sub-vendor", &f->sub_vendor);
	f->nprot = of_property_read_variable_u32_array(np, "avr,protocols", f->protocols, 1, 4);
	if (f->nprot < 0)
		return f->nprot;
	INIT_WORK(&f->work, fake_work);
	f->mbox.dev = &pdev->dev;
	f->mbox.chans = f->chans;
	f->mbox.num_chans = NCHAN;
	f->mbox.ops = &fake_ops;
	return devm_mbox_controller_register(&pdev->dev, &f->mbox);
}

static const struct of_device_id fake_match[] = { { .compatible = "avr,fake-scmi-platform" }, {} };
static struct platform_driver fake_driver = {
	.probe = fake_probe,
	.driver = { .name = "avr-fake-scmi", .of_match_table = fake_match },
};
module_platform_driver(fake_driver);
MODULE_LICENSE("GPL");
