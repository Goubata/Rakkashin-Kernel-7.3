// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/io-pgtable.h>
#include <linux/iommu.h>
#include <linux/module.h>
#include <linux/qcom-io-pgtable.h>
#include <linux/sizes.h>
#include "../../ports/qcom-iommu-util/qcom-io-pgtable-alloc.h"

/* Software page-table tests: these callbacks never touch device registers. */
static void flush_all(void *cookie) { }
static void flush_walk(unsigned long iova,size_t size,size_t granule,void *cookie) { }
static void add_page(struct iommu_iotlb_gather *gather,unsigned long iova,
                     size_t granule,void *cookie) { }
struct pgtable_context {
 struct io_pgtable_ops *ops;
 struct list_head deferred;
};
static void walk_page(void *cookie,void *virt)
{
 struct pgtable_context *ctx=cookie;
 list_add_tail(&virt_to_page(virt)->lru,&ctx->deferred);
}
static void invalidate(void *cookie) { }
static void sync(void *cookie) { }
static void log_table(void *cookie,void *virt,unsigned long iova,size_t granule) { }
static const struct iommu_flush_ops tlb_ops={
 .tlb_flush_all=flush_all,.tlb_flush_walk=flush_walk,.tlb_add_page=add_page,
};
static const struct qcom_iommu_flush_ops qcom_tlb_ops={
 .tlb_add_walk_page=walk_page,.tlb_add_inv=invalidate,.tlb_sync=sync,
};
static const struct qcom_iommu_pgtable_log_ops log_ops={
 .log_new_table=log_table,.log_remove_table=log_table,
};
static void free_table(void *arg)
{
 struct pgtable_context *ctx=arg;
 /* The emulated TLB cannot retain translations. Drain deferred walk pages
  * before destroying the allocator pool, as a real SMMU does after sync. */
 while (!list_empty(&ctx->deferred)) {
  struct page *page=list_first_entry(&ctx->deferred,struct page,lru);
  list_del(&page->lru);
  qcom_io_pgtable_free_page(page);
 }
 qcom_free_io_pgtable_ops(ctx->ops);
}

static void map_unmap_remap(struct kunit *test,enum io_pgtable_fmt format,unsigned int count)
{
 struct qcom_io_pgtable_info info={
  .cfg={.ias=48,.oas=40,.pgsize_bitmap=SZ_4K|SZ_2M|SZ_1G,
        .coherent_walk=true,.tlb=&tlb_ops},
  .iommu_tlb_ops=&qcom_tlb_ops,.pgtable_log_ops=&log_ops,
  .iova_base=0,.iova_end=U32_MAX,
 };
 struct pgtable_context *ctx=kunit_kzalloc(test,sizeof(*ctx),GFP_KERNEL);
 struct io_pgtable_ops *ops;
 struct iommu_iotlb_gather gather;
 const unsigned long base=SZ_64K;
 size_t mapped=0;
 int ret;
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,ctx);
 INIT_LIST_HEAD(&ctx->deferred);
 ops=qcom_alloc_io_pgtable_ops(format,&info,ctx);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,ops);
 ctx->ops=ops;
 KUNIT_ASSERT_EQ(test,kunit_add_action_or_reset(test,free_table,ctx),0);
 iommu_iotlb_gather_init(&gather);
 KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,base),(phys_addr_t)0);
 ret=ops->map_pages(ops,base,SZ_2M,SZ_4K,count,IOMMU_READ|IOMMU_WRITE,GFP_KERNEL,&mapped);
 KUNIT_ASSERT_EQ(test,ret,0);
 KUNIT_ASSERT_EQ(test,mapped,(size_t)(count*SZ_4K));
 for (int i=0;i<count;i++)
  KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,base+i*SZ_4K+42),
                  (phys_addr_t)(SZ_2M+i*SZ_4K+42));
 mapped=0;
 KUNIT_ASSERT_EQ(test,ops->map_pages(ops,2*base,SZ_8M,SZ_4K,1,IOMMU_READ,GFP_KERNEL,&mapped),0);
 KUNIT_EXPECT_EQ(test,ops->unmap_pages(ops,base+SZ_4K,SZ_4K,1,&gather),(size_t)SZ_4K);
 KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,base+SZ_4K+42),(phys_addr_t)0);
 KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,base+42),(phys_addr_t)(SZ_2M+42));
 KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,base+2*SZ_4K+42),(phys_addr_t)(SZ_2M+2*SZ_4K+42));
 mapped=0;
 KUNIT_ASSERT_EQ(test,ops->map_pages(ops,base+SZ_4K,SZ_16M,SZ_4K,1,IOMMU_WRITE,GFP_KERNEL,&mapped),0);
 KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,base+SZ_4K+42),(phys_addr_t)(SZ_16M+42));
 KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,2*base+42),(phys_addr_t)(SZ_8M+42));
 KUNIT_EXPECT_EQ(test,ops->unmap_pages(ops,base,SZ_4K,count,&gather),(size_t)(count*SZ_4K));
 for (int i=0;i<count;i++)
  KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,base+i*SZ_4K),(phys_addr_t)0);
 KUNIT_EXPECT_EQ(test,ops->iova_to_phys(ops,2*base+42),(phys_addr_t)(SZ_8M+42));
 KUNIT_EXPECT_EQ(test,ops->unmap_pages(ops,2*base,SZ_4K,1,&gather),(size_t)SZ_4K);
}
static void fast_mappings_survive_partial_unmap(struct kunit *test)
{
 map_unmap_remap(test,ARM_V8L_FAST,4);
}
static void lpae_mappings_survive_partial_unmap(struct kunit *test)
{
 map_unmap_remap(test,QCOM_ARM_64_LPAE_S1,4);
}
static void lpae_contiguous_group_survives_partial_unmap(struct kunit *test)
{
 map_unmap_remap(test,QCOM_ARM_64_LPAE_S1,16);
}
static struct kunit_case pgtable_cases[]={
 KUNIT_CASE(fast_mappings_survive_partial_unmap),
 KUNIT_CASE(lpae_mappings_survive_partial_unmap),
 KUNIT_CASE(lpae_contiguous_group_survives_partial_unmap),{}
};
static struct kunit_suite pgtable_suite={.name="avr_vendor_io_pgtable",.test_cases=pgtable_cases};
kunit_test_suite(pgtable_suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Vendor FAST and LPAE software page-table regression tests");
