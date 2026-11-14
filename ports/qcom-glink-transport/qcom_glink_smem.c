// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016, Linaro Ltd
 */

#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/mailbox_client.h>
#include <linux/mfd/syscon.h>
#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/idr.h>
#include <linux/circ_buf.h>
#include <linux/soc/qcom/smem.h>
#include <linux/sizes.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/workqueue.h>
#include <linux/list.h>

#include <linux/rpmsg/qcom_glink.h>

#include "qcom_glink_native.h"

#define FIFO_FULL_RESERVE 8
#define FIFO_ALIGNMENT 8
#define TX_BLOCKED_CMD_RESERVE 8 /* size of struct read_notif_request */

#define SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR	478
#define SMEM_GLINK_NATIVE_XPRT_FIFO_0		479
#define SMEM_GLINK_NATIVE_XPRT_FIFO_1		480

struct qcom_glink_smem {
	struct device dev;

	int irq;
	struct qcom_glink *glink;

	struct mbox_client mbox_client;
	struct mbox_chan *mbox_chan;

	u32 remote_pid;
};

struct glink_smem_pipe {
	struct qcom_glink_pipe native;

	__le32 *tail;
	__le32 *head;

	void *fifo;

	struct qcom_glink_smem *smem;
};

#define to_smem_pipe(p) container_of(p, struct glink_smem_pipe, native)

static bool glink_smem_positions(struct glink_smem_pipe *pipe,
				u32 *head, u32 *tail)
{
	size_t length;

	if (!smp_load_acquire(&pipe->fifo) || !pipe->head || !pipe->tail)
		return false;
	length = READ_ONCE(pipe->native.length);
	if (length < FIFO_ALIGNMENT || length > U32_MAX ||
	    !IS_ALIGNED(length, FIFO_ALIGNMENT))
		return false;
	*head = le32_to_cpu(READ_ONCE(*pipe->head));
	*tail = le32_to_cpu(READ_ONCE(*pipe->tail));
	return *head < length && *tail < length &&
		IS_ALIGNED(*head, FIFO_ALIGNMENT) && IS_ALIGNED(*tail, FIFO_ALIGNMENT);
}

static size_t glink_smem_rx_avail(struct qcom_glink_pipe *np)
{
	struct glink_smem_pipe *pipe = to_smem_pipe(np);
	struct qcom_glink_smem *smem = pipe->smem;
	size_t len;
	void *fifo;
	u32 head;
	u32 tail;

	if (!pipe->fifo) {
		fifo = qcom_smem_get(smem->remote_pid,
				     SMEM_GLINK_NATIVE_XPRT_FIFO_1, &len);
		if (IS_ERR(fifo)) {
			pr_err("failed to acquire RX fifo handle: %ld\n",
			       PTR_ERR(fifo));
			return 0;
		}

		if (!fifo || len < FIFO_ALIGNMENT || len > U32_MAX ||
		    !IS_ALIGNED(len, FIFO_ALIGNMENT))
			return 0;
		pipe->native.length = len;
		smp_store_release(&pipe->fifo, fifo);
	}

	if (!glink_smem_positions(pipe, &head, &tail))
		return 0;

	if (head < tail)
		return pipe->native.length - tail + head;
	else
		return head - tail;
}

static void glink_smem_rx_peek(struct qcom_glink_pipe *np,
			       void *data, unsigned int offset, size_t count)
{
	struct glink_smem_pipe *pipe = to_smem_pipe(np);
	size_t available = glink_smem_rx_avail(np), len, position;
	u32 head, tail;

	if (!count)
		return;
	if (!data || offset > available || count > available - offset ||
	    !glink_smem_positions(pipe, &head, &tail))
		return;
	position = (size_t)tail + offset;
	if (position >= pipe->native.length)
		position -= pipe->native.length;

	/* Pair remote head publication with FIFO payload consumption. */
	rmb();
	len = min_t(size_t, count, pipe->native.length - position);
	if (len)
		memcpy_fromio(data, pipe->fifo + position, len);
	if (len != count)
		memcpy_fromio(data + len, pipe->fifo, count - len);
}

static void glink_smem_rx_advance(struct qcom_glink_pipe *np, size_t count)
{
	struct glink_smem_pipe *pipe = to_smem_pipe(np);
	size_t position;
	u32 head, tail;

	if (!IS_ALIGNED(count, FIFO_ALIGNMENT) || count > glink_smem_rx_avail(np) ||
	    !glink_smem_positions(pipe, &head, &tail))
		return;
	position = (size_t)tail + count;
	if (position >= pipe->native.length)
		position -= pipe->native.length;
	WRITE_ONCE(*pipe->tail, cpu_to_le32(position));
}

/* Raw free space includes the reserve for READ_NOTIF. Normal packets use
 * tx_avail(), which keeps both the empty/full gap and notification reserve.
 */
static size_t glink_smem_tx_free(struct glink_smem_pipe *pipe)
{
	u32 head, tail;
	if (!glink_smem_positions(pipe, &head, &tail))
		return 0;
	return tail <= head ? pipe->native.length - head + tail : tail - head;
}

static size_t glink_smem_tx_avail(struct qcom_glink_pipe *np)
{
	size_t avail = glink_smem_tx_free(to_smem_pipe(np));
	if (avail < FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE)
		return 0;
	return avail - FIFO_FULL_RESERVE - TX_BLOCKED_CMD_RESERVE;
}

static unsigned int glink_smem_tx_write_one(struct glink_smem_pipe *pipe,
					    unsigned int head,
					    const void *data, size_t count)
{
	size_t len;

	len = min_t(size_t, count, pipe->native.length - head);
	if (len)
		memcpy(pipe->fifo + head, data, len);

	if (len != count)
		memcpy(pipe->fifo, data + len, count - len);

	head += count;
	if (head >= pipe->native.length)
		head -= pipe->native.length;

	return head;
}

static void glink_smem_tx_write(struct qcom_glink_pipe *glink_pipe,
				const void *hdr, size_t hlen,
				const void *data, size_t dlen)
{
	struct glink_smem_pipe *pipe = to_smem_pipe(glink_pipe);
	unsigned int head;
	size_t available = glink_smem_tx_free(pipe);

	if (available < FIFO_FULL_RESERVE || (!hdr && hlen) || (!data && dlen))
		return;
	available -= FIFO_FULL_RESERVE;
	if (hlen > available || dlen > available - hlen ||
	    ALIGN(hlen + dlen, FIFO_ALIGNMENT) > available)
		return;
	head = le32_to_cpu(READ_ONCE(*pipe->head));

	head = glink_smem_tx_write_one(pipe, head, hdr, hlen);
	head = glink_smem_tx_write_one(pipe, head, data, dlen);

	/* Ensure head is always aligned to 8 bytes */
	head = ALIGN(head, 8);
	if (head >= pipe->native.length)
		head -= pipe->native.length;

	/* Ensure ordering of fifo and head update */
	wmb();

	WRITE_ONCE(*pipe->head, cpu_to_le32(head));
}

static void glink_smem_tx_kick(struct qcom_glink_pipe *glink_pipe)
{
	struct glink_smem_pipe *pipe = to_smem_pipe(glink_pipe);
	struct qcom_glink_smem *smem = pipe->smem;

	mbox_send_message(smem->mbox_chan, NULL);
	mbox_client_txdone(smem->mbox_chan, 0);
}

static irqreturn_t qcom_glink_smem_intr(int irq, void *data)
{
	struct qcom_glink_smem *smem = data;

	qcom_glink_native_rx(smem->glink);

	return IRQ_HANDLED;
}

static void qcom_glink_smem_release(struct device *dev)
{
	struct qcom_glink_smem *smem = container_of(dev, struct qcom_glink_smem, dev);

	of_node_put(dev->of_node);
	kfree(smem);
}

struct qcom_glink_smem *qcom_glink_smem_register(struct device *parent,
						 struct device_node *node)
{
	struct glink_smem_pipe *rx_pipe;
	struct glink_smem_pipe *tx_pipe;
	struct qcom_glink_smem *smem;
	struct qcom_glink *glink;
	struct device *dev;
	u32 remote_pid;
	__le32 *descs;
	size_t size;
	int ret;

	if (!parent || !parent->parent || !node)
		return ERR_PTR(-EINVAL);

	smem = kzalloc_obj(*smem);
	if (!smem)
		return ERR_PTR(-ENOMEM);

	dev = &smem->dev;

	dev->parent = parent;
	dev->of_node = of_node_get(node);
	dev->release = qcom_glink_smem_release;
	dev_set_name(dev, "%s:%pOFn", dev_name(parent->parent), node);
	ret = device_register(dev);
	if (ret) {
		pr_err("failed to register glink edge\n");
		put_device(dev);
		return ERR_PTR(ret);
	}

	ret = of_property_read_u32(dev->of_node, "qcom,remote-pid",
				   &remote_pid);
	if (ret) {
		dev_err(dev, "failed to parse qcom,remote-pid\n");
		goto err_put_dev;
	}

	smem->remote_pid = remote_pid;

	rx_pipe = devm_kzalloc(dev, sizeof(*rx_pipe), GFP_KERNEL);
	tx_pipe = devm_kzalloc(dev, sizeof(*tx_pipe), GFP_KERNEL);
	if (!rx_pipe || !tx_pipe) {
		ret = -ENOMEM;
		goto err_put_dev;
	}

	ret = qcom_smem_alloc(remote_pid,
			      SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR, 32);
	if (ret && ret != -EEXIST) {
		dev_err(dev, "failed to allocate glink descriptors\n");
		goto err_put_dev;
	}

	descs = qcom_smem_get(remote_pid,
			      SMEM_GLINK_NATIVE_XPRT_DESCRIPTOR, &size);
	if (IS_ERR(descs)) {
		dev_err(dev, "failed to acquire xprt descriptor\n");
		ret = PTR_ERR(descs);
		goto err_put_dev;
	}

	if (size != 32) {
		dev_err(dev, "glink descriptor of invalid size\n");
		ret = -EINVAL;
		goto err_put_dev;
	}

	tx_pipe->tail = &descs[0];
	tx_pipe->head = &descs[1];
	rx_pipe->tail = &descs[2];
	rx_pipe->head = &descs[3];

	ret = qcom_smem_alloc(remote_pid, SMEM_GLINK_NATIVE_XPRT_FIFO_0,
			      SZ_16K);
	if (ret && ret != -EEXIST) {
		dev_err(dev, "failed to allocate TX fifo\n");
		goto err_put_dev;
	}

	tx_pipe->fifo = qcom_smem_get(remote_pid, SMEM_GLINK_NATIVE_XPRT_FIFO_0,
				      &tx_pipe->native.length);
	if (IS_ERR(tx_pipe->fifo)) {
		dev_err(dev, "failed to acquire TX fifo\n");
		ret = PTR_ERR(tx_pipe->fifo);
		goto err_put_dev;
	}

	if (tx_pipe->native.length < FIFO_FULL_RESERVE + TX_BLOCKED_CMD_RESERVE + FIFO_ALIGNMENT ||
	    tx_pipe->native.length > U32_MAX ||
	    !IS_ALIGNED(tx_pipe->native.length, FIFO_ALIGNMENT)) {
		ret = -EINVAL;
		goto err_put_dev;
	}

	smem->irq = of_irq_get(smem->dev.of_node, 0);
	if (smem->irq <= 0) {
		ret = smem->irq ?: -EINVAL;
		goto err_put_dev;
	}
	ret = devm_request_irq(&smem->dev, smem->irq, qcom_glink_smem_intr,
			       IRQF_NO_SUSPEND | IRQF_NO_AUTOEN,
			       "glink-smem", smem);
	if (ret) {
		dev_err(&smem->dev, "failed to request IRQ\n");
		goto err_put_dev;
	}

	smem->mbox_client.dev = &smem->dev;
	smem->mbox_client.knows_txdone = true;
	smem->mbox_chan = mbox_request_channel(&smem->mbox_client, 0);
	if (IS_ERR(smem->mbox_chan)) {
		ret = dev_err_probe(&smem->dev, PTR_ERR(smem->mbox_chan),
				    "failed to acquire IPC channel\n");
		goto err_put_dev;
	}

	rx_pipe->smem = smem;
	rx_pipe->native.avail = glink_smem_rx_avail;
	rx_pipe->native.peek = glink_smem_rx_peek;
	rx_pipe->native.advance = glink_smem_rx_advance;

	tx_pipe->smem = smem;
	tx_pipe->native.avail = glink_smem_tx_avail;
	tx_pipe->native.write = glink_smem_tx_write;
	tx_pipe->native.kick = glink_smem_tx_kick;

	*rx_pipe->tail = 0;
	*tx_pipe->head = 0;

	glink = qcom_glink_native_probe(dev,
					GLINK_FEATURE_INTENT_REUSE,
					&rx_pipe->native, &tx_pipe->native,
					false);
	if (IS_ERR(glink)) {
		ret = PTR_ERR(glink);
		goto err_free_mbox;
	}

	smem->glink = glink;

	enable_irq(smem->irq);

	return smem;

err_free_mbox:
	mbox_free_channel(smem->mbox_chan);

err_put_dev:
	device_unregister(dev);

	return ERR_PTR(ret);
}
EXPORT_SYMBOL_GPL(qcom_glink_smem_register);

void qcom_glink_smem_unregister(struct qcom_glink_smem *smem)
{
	struct qcom_glink *glink = smem->glink;

	disable_irq(smem->irq);

	qcom_glink_native_remove(glink);

	mbox_free_channel(smem->mbox_chan);
	device_unregister(&smem->dev);
}
EXPORT_SYMBOL_GPL(qcom_glink_smem_unregister);

MODULE_AUTHOR("Bjorn Andersson <bjorn.andersson@linaro.org>");
MODULE_DESCRIPTION("Qualcomm GLINK SMEM driver");
MODULE_LICENSE("GPL v2");
