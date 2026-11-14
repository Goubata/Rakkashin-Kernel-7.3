/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_ICE_POLICY_H
#define AVR_ICE_POLICY_H
#include <linux/errno.h>
#include <linux/limits.h>
#include <linux/types.h>
static inline int avr_ice_hwkm_slot(int slot, bool use_hwkm, u8 version,
				   bool initialized, u32 ipcat_version)
{
	int offset = initialized && ipcat_version >= 0x02000000 &&
			ipcat_version < 0x02010000 ? 10 : 0;

	if (slot < 0)
		return -EINVAL;
	if (!use_hwkm || version == 1)
		return slot;
	if (slot > (INT_MAX - offset) / 2)
		return -ERANGE;
	return slot * 2 + offset;
}
/* Check before any CFGE/MMIO write or secure firmware request. */
static inline int avr_ice_wrapped_ready(bool use_hwkm, bool initialized,
				      size_t key_size, u8 data_unit, int slot)
{
	if (!key_size || key_size > 128 || !data_unit ||
	    (data_unit & (data_unit - 1)) || slot < 0)
		return -EINVAL;
	return use_hwkm && !initialized ? -EAGAIN : 0;
}
#endif
