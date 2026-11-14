/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_RM_NOTIFIERS_H
#define AVR_RM_NOTIFIERS_H
#include <linux/errno.h>
#include <linux/notifier.h>

/* Probe/remove serialize this state. Unregister functions must synchronize
 * in-flight callbacks, as the production SRCU/blocking notifier APIs do.
 */
struct avr_rm_notifier_ops {
	int (*register_first)(struct notifier_block *);
	int (*unregister_first)(struct notifier_block *);
	int (*register_second)(struct notifier_block *);
	int (*unregister_second)(struct notifier_block *);
};
struct avr_rm_notifiers {
	const struct avr_rm_notifier_ops *ops;
	struct notifier_block *first, *second;
	bool first_registered, second_registered;
};
static inline int avr_rm_notifiers_register(struct avr_rm_notifiers *state)
{
	int ret;

	if (state->first_registered || state->second_registered)
		return -EALREADY;
	ret = state->ops->register_first(state->first);
	if (ret)
		return ret;
	state->first_registered = true;
	if (!state->second)
		return 0;
	ret = state->ops->register_second(state->second);
	if (ret) {
		state->ops->unregister_first(state->first);
		state->first_registered = false;
		return ret;
	}
	state->second_registered = true;
	return 0;
}
static inline void avr_rm_notifiers_unregister(struct avr_rm_notifiers *state)
{
	if (state->second_registered) {
		state->ops->unregister_second(state->second);
		state->second_registered = false;
	}
	if (state->first_registered) {
		state->ops->unregister_first(state->first);
		state->first_registered = false;
	}
}
#endif
