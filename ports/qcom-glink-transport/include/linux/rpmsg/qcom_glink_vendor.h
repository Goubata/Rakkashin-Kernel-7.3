/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AVR 7.3 port: android16-6.12 qcom-soc GLINK client extensions used by vendor
 * clients (glink_pkt: modem AT/DATA control channels, ADSP APR). Implemented in
 * ports/qcom-glink-transport/qcom_glink_native.c on the 7.3-based transport.
 * Signals use TIOCM_* values like 6.12. Zero-copy RX (RPMSG_DEFER +
 * qcom_glink_rx_done) is not offered: rx_done_supported() reports false and
 * clients use their copying receive path.
 */
#ifndef _LINUX_RPMSG_QCOM_GLINK_VENDOR_H
#define _LINUX_RPMSG_QCOM_GLINK_VENDOR_H

#include <linux/rpmsg.h>

#ifndef RPMSG_DEFER
#define RPMSG_DEFER	1
#endif

int qcom_glink_get_signals(struct rpmsg_endpoint *ept);
int qcom_glink_set_signals(struct rpmsg_endpoint *ept, u32 set, u32 clear);
int qcom_glink_register_signals_cb(struct rpmsg_endpoint *ept,
				   int (*cb)(struct rpmsg_device *, void *, u32, u32));
bool qcom_glink_rx_done_supported(struct rpmsg_endpoint *ept);
int qcom_glink_rx_done(struct rpmsg_endpoint *ept, void *data);
bool qcom_glink_is_wakeup(bool reset);

#endif
