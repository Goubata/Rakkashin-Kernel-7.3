/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_PAS_PROXY_H
#define AVR_PAS_PROXY_H
#include <linux/mutex.h>
#include <linux/bits.h>
enum avr_pas_proxy_step {
	AVR_PROXY_PDS, AVR_PROXY_XO, AVR_PROXY_AGGRE,
	AVR_PROXY_CX, AVR_PROXY_PX, AVR_PROXY_ARC, AVR_PROXY_COUNT
};
struct avr_pas_proxy_ops {
	int (*acquire)(void *ctx, unsigned int step);
	int (*release)(void *ctx, unsigned int step);
};
struct avr_pas_proxy {
	struct mutex lock;
	const struct avr_pas_proxy_ops *ops;
	void *ctx;
	unsigned long owned;
	bool handover_pending, handover_allowed;
	int cleanup_error;
};
static int avr_pas_proxy_release_locked(struct avr_pas_proxy *proxy)
{
	int i, ret;
	for (i = AVR_PROXY_COUNT - 1; i >= 0; i--) {
		if (!(proxy->owned & BIT(i)))
			continue;
		ret = proxy->ops->release(proxy->ctx, i);
		if (ret)
			return ret;
		proxy->owned &= ~BIT(i);
	}
	proxy->handover_allowed = false;
	return 0;
}
static int avr_pas_proxy_begin(struct avr_pas_proxy *proxy)
{
	int ret = 0;
	mutex_lock(&proxy->lock);
	if (proxy->owned)
		ret = -EBUSY;
	else {
		proxy->handover_pending = false;
		proxy->handover_allowed = false;
		proxy->cleanup_error = 0;
	}
	mutex_unlock(&proxy->lock);
	return ret;
}
static int avr_pas_proxy_acquire(struct avr_pas_proxy *proxy)
{
	int i, ret = 0;
	mutex_lock(&proxy->lock);
	if (proxy->owned) {
		ret = -EBUSY;
		goto out;
	}
	for (i = 0; i < AVR_PROXY_COUNT; i++) {
		/* ARC acquisition can fail with a retained partial vote. */
		if (i == AVR_PROXY_ARC)
			proxy->owned |= BIT(i);
		ret = proxy->ops->acquire(proxy->ctx, i);
		if (ret) {
			proxy->cleanup_error = avr_pas_proxy_release_locked(proxy);
			goto out;
		}
		proxy->owned |= BIT(i);
	}
out:
	mutex_unlock(&proxy->lock);
	return ret;
}
static int avr_pas_proxy_release(struct avr_pas_proxy *proxy)
{
	int ret;
	mutex_lock(&proxy->lock);
	ret = avr_pas_proxy_release_locked(proxy);
	proxy->cleanup_error = ret;
	mutex_unlock(&proxy->lock);
	return ret;
}
static int avr_pas_proxy_handover(struct avr_pas_proxy *proxy)
{
	int ret = 0;
	mutex_lock(&proxy->lock);
	proxy->handover_pending = true;
	if (proxy->handover_allowed) {
		ret = avr_pas_proxy_release_locked(proxy);
		proxy->cleanup_error = ret;
	}
	mutex_unlock(&proxy->lock);
	return ret;
}
static int avr_pas_proxy_allow_handover(struct avr_pas_proxy *proxy)
{
	int ret = 0;
	mutex_lock(&proxy->lock);
	proxy->handover_allowed = true;
	if (proxy->handover_pending) {
		ret = avr_pas_proxy_release_locked(proxy);
		proxy->cleanup_error = ret;
	}
	mutex_unlock(&proxy->lock);
	return ret;
}
#endif
