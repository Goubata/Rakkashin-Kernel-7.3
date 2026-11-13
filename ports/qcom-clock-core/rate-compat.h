/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_QCOM_RATE_COMPAT_H
#define AVR_QCOM_RATE_COMPAT_H

#include <linux/clk-provider.h>

typedef long (*qcom_vendor_round_rate_fn)(struct clk_hw *, unsigned long,
					unsigned long *);

static inline int qcom_vendor_determine_rate(struct clk_hw *hw,
		struct clk_rate_request *req, qcom_vendor_round_rate_fn round_rate)
{
	long rate = round_rate(hw, req->rate, &req->best_parent_rate);

	if (rate < 0)
		return rate;
	req->rate = rate;
	return 0;
}

/* Preserve the old divider API's return value and parent-rate output while
 * delegating the calculation to the target kernel's divider implementation.
 */
static inline long qcom_vendor_divider_round_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long *parent_rate,
		const struct clk_div_table *table, u8 width, unsigned long flags)
{
	struct clk_rate_request req;
	int ret;

	clk_hw_init_rate_request(hw, &req, rate);
	req.best_parent_rate = *parent_rate;
	ret = divider_determine_rate(hw, &req, table, width, flags);
	if (ret)
		return ret;
	*parent_rate = req.best_parent_rate;
	return req.rate;
}
static inline long qcom_vendor_divider_ro_round_rate(struct clk_hw *hw,
		unsigned long rate, unsigned long *parent_rate,
		const struct clk_div_table *table, u8 width, unsigned long flags,
		unsigned int value)
{
	struct clk_rate_request req;
	int ret;

	clk_hw_init_rate_request(hw, &req, rate);
	req.best_parent_rate = *parent_rate;
	ret = divider_ro_determine_rate(hw, &req, table, width, flags, value);
	if (ret)
		return ret;
	*parent_rate = req.best_parent_rate;
	return req.rate;
}
#endif
