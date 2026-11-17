/* SPDX-License-Identifier: GPL-2.0-only */
/* AVR 7.3: the only difference of the vendor <linux/slimbus.h> (qcom-soc 037aa1c)
 * from 7.3's: this stream API (drivers/slimbus/stream.c), used by btfm_slim_codec. */
#ifndef AVR_SLIMBUS_VENDOR_H
#define AVR_SLIMBUS_VENDOR_H
#include <linux/slimbus.h>
int slim_stream_unprepare_disconnect_port(struct slim_stream_runtime *stream,
		bool disconnect_ports, bool is_session_completed);
#endif
