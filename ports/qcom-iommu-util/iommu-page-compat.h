/* SPDX-License-Identifier: GPL-2.0-only */
#ifndef AVR_IOMMU_PAGE_COMPAT_H
#define AVR_IOMMU_PAGE_COMPAT_H
#include "drivers/iommu/iommu-pages.h"
/* Use the target allocator so IOMMU/memcg page accounting is retained. */
static inline struct page *avr_iommu_alloc_page(gfp_t gfp)
{
	void *virt = iommu_alloc_pages_sz(gfp, PAGE_SIZE);

	return virt ? virt_to_page(virt) : NULL;
}
static inline void avr_iommu_free_page(struct page *page)
{
	iommu_free_pages(page_address(page));
}
#endif
