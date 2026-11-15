/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_PAS_RAILS_H
#define AVR_PAS_RAILS_H
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#define AVR_PAS_RAIL_COUNT 2
struct avr_pas_rail {
	struct regulator *reg;
	int level, load;
	bool voltage_set, load_set, enabled;
};
struct avr_pas_rails {
	struct mutex lock;
	struct avr_pas_rail rail[AVR_PAS_RAIL_COUNT];
	int cleanup_error;
};
static int avr_pas_rails_release_locked(struct avr_pas_rails *rails)
{
	int i, ret;
	for (i = AVR_PAS_RAIL_COUNT - 1; i >= 0; i--) {
		struct avr_pas_rail *r = &rails->rail[i];
		if (r->enabled) {
			ret = regulator_disable(r->reg);
			if (ret) return ret;
			r->enabled = false;
		}
		if (r->load_set) {
			ret = regulator_set_load(r->reg, 0);
			if (ret < 0) return ret;
			r->load_set = false;
		}
		if (r->voltage_set) {
			ret = regulator_set_voltage(r->reg, 0, INT_MAX);
			if (ret) return ret;
			r->voltage_set = false;
		}
	}
	return 0;
}
static int avr_pas_rails_release(struct avr_pas_rails *rails)
{
	int ret;
	mutex_lock(&rails->lock);
	ret = avr_pas_rails_release_locked(rails);
	rails->cleanup_error = ret;
	mutex_unlock(&rails->lock);
	return ret;
}
static int avr_pas_rails_enable(struct avr_pas_rails *rails)
{
	int i, ret;
	mutex_lock(&rails->lock);
	for (i = 0; i < AVR_PAS_RAIL_COUNT; i++) {
		struct avr_pas_rail *r = &rails->rail[i];
		if (r->enabled || r->load_set || r->voltage_set) {
			ret = -EBUSY;
			goto out;
		}
		if (IS_ERR_OR_NULL(r->reg) || r->level < 0 || r->load < 0) {
			ret = -EINVAL;
			goto out;
		}
	}
	rails->cleanup_error = 0;
	for (i = 0; i < AVR_PAS_RAIL_COUNT; i++) {
		struct avr_pas_rail *r = &rails->rail[i];
		ret = regulator_set_voltage(r->reg, r->level, INT_MAX);
		if (ret) goto unwind;
		r->voltage_set = true;
		ret = regulator_set_load(r->reg, r->load);
		/* set_load may return a nonnegative operating mode, not just zero. */
		if (ret < 0) goto unwind;
		r->load_set = true;
		ret = regulator_enable(r->reg);
		if (ret) goto unwind;
		r->enabled = true;
	}
	ret = 0;
	goto out;
unwind:
	rails->cleanup_error = avr_pas_rails_release_locked(rails);
out:
	mutex_unlock(&rails->lock);
	return ret;
}
#endif
