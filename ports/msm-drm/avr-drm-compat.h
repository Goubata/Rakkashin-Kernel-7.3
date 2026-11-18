/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * AVR 7.3 port: android16-6.12 DRM API names used by the canoe display driver.
 * android18-7.3 renamed struct drm_atomic_state -> struct drm_atomic_commit and
 * its helpers drm_atomic_state_* -> drm_atomic_commit_* (same layout/semantics);
 * the drm_mode_config_funcs hook names (atomic_state_alloc/clear/free) kept their
 * names. Vendor files also relied on <drm/drm_print.h> arriving indirectly.
 */
#ifndef AVR_DRM_COMPAT_H
#define AVR_DRM_COMPAT_H

#define drm_atomic_state		drm_atomic_commit
#define drm_atomic_state_alloc		drm_atomic_commit_alloc
#define drm_atomic_state_clear		drm_atomic_commit_clear
#define drm_atomic_state_get		drm_atomic_commit_get
#define drm_atomic_state_put		drm_atomic_commit_put
#define drm_atomic_state_init		drm_atomic_commit_init
#define drm_atomic_state_default_clear	drm_atomic_commit_default_clear
#define drm_atomic_state_default_release drm_atomic_commit_default_release
#define __drm_atomic_state_free		__drm_atomic_commit_free

#include <linux/device.h>
#include <drm/drm_print.h>


/* clk_ops.round_rate was removed in 7.x; 6.12 clk core called it as
 * rate = round_rate(hw, req->rate, &req->best_parent_rate); req->rate = rate. */
#include <linux/clk-provider.h>
#define AVR_CLK_DETERMINE_FROM_ROUND(round)					\
static int round##_avr_determine(struct clk_hw *hw, struct clk_rate_request *req) \
{									\
	long rate = round(hw, req->rate, &req->best_parent_rate);		\
									\
	if (rate < 0)							\
		return rate;						\
	req->rate = rate;						\
	return 0;							\
}

/* drm_bridge_funcs lost the non-atomic enable/disable/pre_enable/post_disable
 * hooks in 7.x; 6.12 drm_atomic_bridge_chain_*() fell back to them when the
 * atomic variant was absent, so the wrapper keeps that behaviour. attach()
 * gained the encoder argument (bridge->encoder is still set before the call). */
#include <drm/drm_bridge.h>
#define AVR_BRIDGE_ATOMIC(fn)							\
static void fn##_avr_atomic(struct drm_bridge *bridge,			\
			    struct drm_atomic_commit *state)		\
{									\
	fn(bridge);							\
}
#define AVR_BRIDGE_ATTACH(fn)							\
static int fn##_avr_attach(struct drm_bridge *bridge,			\
			   struct drm_encoder *encoder,			\
			   enum drm_bridge_attach_flags flags)		\
{									\
	return fn(bridge, flags);					\
}

/* 7.x bridges are refcounted: drm_bridge_attach()/drm_bridge_add() take references and
 * the last drm_bridge_put() frees bridge->container. A kzalloc()ed 6.12-style bridge has
 * refcount 0 and no container ("DRM bridge corrupted or not allocated by
 * devm_drm_bridge_alloc()", then refcount_t addition on 0 and an oops in phone test 26).
 * Give msm_drm's bridges what devm_drm_bridge_alloc() would (container, kref, list) and
 * drop the allocation reference instead of kfree() on the error/cleanup paths. Not devm:
 * msm_drm frees its bridges explicitly from its own unbind/cleanup code. */
static inline void avr_drm_bridge_init(struct drm_bridge *bridge, void *container,
				       const struct drm_bridge_funcs *funcs)
{
	INIT_LIST_HEAD(&bridge->list);
	bridge->container = container;
	bridge->funcs = funcs;
	kref_init(&bridge->refcount);
	drm_bridge_add(bridge);
}

/* 7.x drm_bridge_attach() always creates the bridge's atomic private state through
 * funcs->atomic_create_state (6.12 only did so for bridges that opted in). msm_drm's
 * bridges have none: the kCFI-checked indirect call through NULL faulted at
 * 0xfffffffffffffffc in drm_bridge_atomic_create_priv_state (phone test 28). The plain
 * helpers are what a bridge without its own state needs. */
#include <drm/drm_atomic_state_helper.h>
#define AVR_BRIDGE_STATE_HELPERS						\
	.atomic_create_state = drm_atomic_helper_bridge_create_state,		\
	.atomic_duplicate_state = drm_atomic_helper_bridge_duplicate_state,	\
	.atomic_destroy_state = drm_atomic_helper_bridge_destroy_state

/* For bridges embedded in a larger driver structure (dp_mst bridge array): never let the
 * refcount free them, so pin one extra reference. */
static inline void avr_drm_bridge_init_embedded(struct drm_bridge *bridge,
						const struct drm_bridge_funcs *funcs)
{
	avr_drm_bridge_init(bridge, bridge, funcs);
	drm_bridge_get(bridge);
}

static inline void avr_drm_bridge_release(struct drm_bridge *bridge)
{
	drm_bridge_remove(bridge);
	drm_bridge_put(bridge);
}

/* strncpy() was removed from the 7.x kernel; exact C semantics (no forced
 * NUL termination, zero padding) as the 6.12 lib/string.c version. */
#include <linux/types.h>
static inline char *avr_strncpy(char *dest, const char *src, size_t count)
{
	size_t i;

	for (i = 0; i < count && src[i]; i++)
		dest[i] = src[i];
	for (; i < count; i++)
		dest[i] = '\0';
	return dest;
}
#define strncpy avr_strncpy

#include <linux/hex.h>

/* devm_gpio_request() was removed; 6.12 = gpio_request() + devres gpio_free() */
#include <linux/gpio/legacy.h>
static inline void avr_devm_gpio_free(void *gpio)
{
	gpio_free((unsigned long)gpio);
}
static inline int avr_devm_gpio_request(struct device *dev, unsigned int gpio,
					const char *label)
{
	int rc = gpio_request(gpio, label);

	if (rc)
		return rc;
	return devm_add_action_or_reset(dev, avr_devm_gpio_free,
					(void *)(unsigned long)gpio);
}
#define devm_gpio_request avr_devm_gpio_request

/* deprecated 6.12 inline returning planes[i].state, the field 7.3 renamed
 * state_to_destroy (new state before swap, old state after it) */
#define drm_atomic_get_existing_plane_state(state, plane) \
	((state)->planes[drm_plane_index(plane)].state_to_destroy)
#define drm_atomic_get_existing_crtc_state(state, crtc) \
	((state)->crtcs[drm_crtc_index(crtc)].state_to_destroy)

/* 6.12 drm_wait_one_vblank(): 100 ms timeout, WARN on timeout (7.3 only has
 * the crtc variant with a 1 s timeout). */
#include <drm/drm_crtc.h>
#include <drm/drm_vblank.h>
static inline void drm_wait_one_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct drm_crtc *crtc = drm_crtc_from_index(dev, pipe);
	long ret;
	u64 last;

	if (drm_WARN_ON(dev, !crtc))
		return;
	ret = drm_crtc_vblank_get(crtc);
	if (drm_WARN(dev, ret, "vblank not available on crtc %i, ret=%li\n", pipe, ret))
		return;
	last = drm_crtc_vblank_count(crtc);
	ret = wait_event_timeout(*drm_crtc_vblank_waitqueue(crtc),
				 last != drm_crtc_vblank_count(crtc),
				 msecs_to_jiffies(100));
	drm_WARN(dev, ret == 0, "vblank wait timed out on crtc %i\n", pipe);
	drm_crtc_vblank_put(crtc);
}

/* drm_panel_init() became static in 7.x (panels are devm_drm_panel_alloc()'ed
 * and refcounted). dsi_panel embeds its drm_panel, so do the 6.12 init and
 * hold one permanent reference with no container: drm_panel_add/remove
 * get/put never reach zero (and kfree(NULL) would be the only release). */
#include <drm/drm_panel.h>
static inline void drm_panel_init(struct drm_panel *panel, struct device *dev,
				  const struct drm_panel_funcs *funcs,
				  int connector_type)
{
	INIT_LIST_HEAD(&panel->list);
	INIT_LIST_HEAD(&panel->followers);
	mutex_init(&panel->follower_lock);
	panel->dev = dev;
	panel->funcs = funcs;
	panel->connector_type = connector_type;
	panel->container = NULL;
	kref_init(&panel->refcount);
}

#endif
