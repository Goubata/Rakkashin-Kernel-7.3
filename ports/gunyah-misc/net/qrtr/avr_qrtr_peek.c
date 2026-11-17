// SPDX-License-Identifier: GPL-2.0-only
/*
 * AVR 7.3: the phone runs the native android18-7.3 qrtr.ko, which does not
 * export the vendor qrtr_peek_pkt_size(). qrtr-gunyah only needs it to size
 * fragmented packets, so carry the vendor af_qrtr.c implementation (wire
 * header layouts identical in 7.3 net/qrtr/af_qrtr.c) inside this module.
 */
#include <linux/kernel.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/align.h>

#define QRTR_PROTO_VER_1 1
#define QRTR_PROTO_VER_2 3

struct qrtr_hdr_v1 {
	__le32 version;
	__le32 type;
	__le32 src_node_id;
	__le32 src_port_id;
	__le32 confirm_rx;
	__le32 size;
	__le32 dst_node_id;
	__le32 dst_port_id;
} __packed;

struct qrtr_hdr_v2 {
	u8 version;
	u8 type;
	u8 flags;
	u8 optlen;
	__le32 size;
	__le16 src_node_id;
	__le16 src_port_id;
	__le16 dst_node_id;
	__le16 dst_port_id;
};

int qrtr_peek_pkt_size(const void *data);
int qrtr_peek_pkt_size(const void *data)
{
	const struct qrtr_hdr_v1 *v1;
	const struct qrtr_hdr_v2 *v2;
	unsigned int hdrlen, size;

	switch (*(const u8 *)data) {
	case QRTR_PROTO_VER_1:
		v1 = data;
		hdrlen = sizeof(*v1);
		size = le32_to_cpu(v1->size);
		break;
	case QRTR_PROTO_VER_2:
		v2 = data;
		hdrlen = sizeof(*v2) + v2->optlen;
		size = le32_to_cpu(v2->size);
		break;
	default:
		pr_err("qrtr: Invalid version %d\n", *(const u8 *)data);
		return -EINVAL;
	}
	return ALIGN(size, 4) + hdrlen;
}
