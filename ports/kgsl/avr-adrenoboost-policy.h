/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_ADRENOBOOST_POLICY_H
#define AVR_ADRENOBOOST_POLICY_H
#include <linux/minmax.h>
#include <linux/types.h>

/* Lower indices mean higher frequency. Reapply the current cap AFTER retaining
 * an old floor; an old request must never override a newer thermal/max cap.
 */
static inline int avr_adrenoboost_clamp(int request, int cap, int user_floor)
{
	if (request < 0)
		return -1;
	request = max(request, cap);
	return request >= user_floor ? -1 : request;
}

static inline int avr_adrenoboost_select(int active, unsigned int level,
		int previous, int cap, int user_floor, bool saturated, bool hold)
{
	int request = -1;

	if (!level)
		return -1;
	level = min(level, 3U);
	if (saturated) {
		request = max(active - (int)level, cap);
		if (previous >= 0)
			request = min(request, previous);
	} else if (hold) {
		request = previous;
	}
	return avr_adrenoboost_clamp(request, cap, user_floor);
}
#endif
