/* SPDX-License-Identifier: GPL-2.0-only */
/* AVR compatibility definitions reconstructed from the IPCLite driver,
 * pinned canoe-synx.dtsi and 42 saved DT mailbox/interrupt entries.
 * Slot indices and hardware signal IDs were checked independently; their
 * equal values on this saved device are not an assumed general rule.
 * This is owned compatibility data, not the recovered original vendor header.
 */
#ifndef AVR_DT_IPCLITE_SIGNALS_H
#define AVR_DT_IPCLITE_SIGNALS_H
#define IPCLITE_MSG_SIGNAL 0
#define IPCLITE_MEM_INIT_SIGNAL 1
#define IPCLITE_VERSION_SIGNAL 2
#define IPCLITE_TEST_SIGNAL 3
#define IPCLITE_SSR_SIGNAL 4
#define IPCLITE_DEBUG_SIGNAL 5
#define MAX_CHANNEL_SIGNALS 6

/* COMPUTE_L0 hardware signal IDs observed on AVR. */
#define IPCC_COMPUTE_L0_SIGNAL_IPCLITE_DEBUG 5
#define IPCC_COMPUTE_L0_SIGNAL_IPCLITE_MEM_INIT 1
#define IPCC_COMPUTE_L0_SIGNAL_IPCLITE_MSG 0
#define IPCC_COMPUTE_L0_SIGNAL_IPCLITE_SSR 4
#define IPCC_COMPUTE_L0_SIGNAL_IPCLITE_TEST 3
#define IPCC_COMPUTE_L0_SIGNAL_IPCLITE_VERSION 2
#endif
