/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_FENCE_COMPAT_H
#define AVR_FENCE_COMPAT_H
#include <linux/dma-fence.h>
#include <linux/dma-fence-array.h>
/* Preserve the old caller's already-signalled error under its fence lock. */
static inline int avr_dma_fence_signal_locked(struct dma_fence *fence)
{
 lockdep_assert_held(dma_fence_spinlock(fence));
 if (test_bit(DMA_FENCE_FLAG_SIGNALED_BIT, &fence->flags))
  return -EINVAL;
 dma_fence_signal_locked(fence);
 return 0;
}
static inline int avr_dma_fence_signal(struct dma_fence *fence)
{
 unsigned long flags;
 int ret;
 dma_fence_lock_irqsave(fence, flags);
 ret = avr_dma_fence_signal_locked(fence);
 dma_fence_unlock_irqrestore(fence, flags);
 return ret;
}
/* Before publication no callbacks can change num_pending. The target array
 * callbacks/signaled method still support a pending count of one for ANY. */
static inline struct dma_fence_array *avr_dma_fence_array_create(int count,
 struct dma_fence **fences, u64 context, unsigned int seqno, bool any)
{
 struct dma_fence_array *array = dma_fence_array_create(count, fences, context, seqno);
 if (array && any)
  atomic_set(&array->num_pending, 1);
 return array;
}
#endif
