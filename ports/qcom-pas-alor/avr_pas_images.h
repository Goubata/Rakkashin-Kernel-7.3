/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_PAS_IMAGES_H
#define AVR_PAS_IMAGES_H
#include <linux/types.h>
#include <linux/errno.h>
/* The remoteproc operation mutex serializes this state. IRQ handover owns only
 * proxy resources, and never changes secure image or mapping ownership. */
enum avr_pas_image { AVR_IMAGE_MAIN, AVR_IMAGE_DTB, AVR_IMAGE_COUNT };
struct avr_pas_images {
	bool needs_shutdown[AVR_IMAGE_COUNT];
	bool mapped[AVR_IMAGE_COUNT];
};
struct avr_pas_image_ops {
	int (*shutdown)(void *ctx, unsigned int image);
	void (*unmap)(void *ctx, unsigned int image);
};
static bool avr_pas_images_active(const struct avr_pas_images *images)
{
	return images->needs_shutdown[AVR_IMAGE_MAIN] || images->needs_shutdown[AVR_IMAGE_DTB];
}
static bool avr_pas_images_busy(const struct avr_pas_images *images)
{
	return avr_pas_images_active(images) || images->mapped[AVR_IMAGE_MAIN] || images->mapped[AVR_IMAGE_DTB];
}
static int avr_pas_images_quiesce(struct avr_pas_images *images,
				 const struct avr_pas_image_ops *ops, void *ctx)
{
	unsigned int i;
	int ret;
	/* Stop executable code before its authenticated DTB dependency. */
	for (i = AVR_IMAGE_MAIN; i < AVR_IMAGE_COUNT; i++) {
		if (!images->needs_shutdown[i])
			continue;
		ret = ops->shutdown(ctx, i);
		if (ret)
			return ret;
		images->needs_shutdown[i] = false;
	}
	return 0;
}
static int avr_pas_images_unmap(struct avr_pas_images *images,
			       const struct avr_pas_image_ops *ops, void *ctx)
{
	unsigned int i;
	if (avr_pas_images_active(images))
		return -EBUSY;
	for (i = AVR_IMAGE_MAIN; i < AVR_IMAGE_COUNT; i++) {
		if (!images->mapped[i])
			continue;
		ops->unmap(ctx, i);
		images->mapped[i] = false;
	}
	return 0;
}
#endif
