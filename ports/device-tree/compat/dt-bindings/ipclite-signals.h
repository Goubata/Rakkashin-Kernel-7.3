/* SPDX-License-Identifier: GPL-2.0-only */
/* AVR reconstruction of signal SLOT indices from the migrated IPCLite driver
 * and 42 saved DT signal entries. Hardware IPCC signal numbers are separately
 * supplied by the pinned qcom-ipcc-canoe.h, not by these slot indices.
 * This is owned compatibility data; the absent vendor header is not recovered.
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
#endif
