// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/firmware/qcom/qcom_scm.h>

/* Execute the production batching/rollback code with only its firmware
 * transport replaced. No secure-world ownership changes happen in QEMU. */
static struct kunit *active_test;
static unsigned int calls, fail_at;
static unsigned int observed_regions[8], observed_sources[8];
static int fake_assign(struct qcom_scm_mem_map_info *regions, size_t regions_size,
                       u32 *src, size_t src_size,
                       struct qcom_scm_current_perm_info *dst, size_t dst_size)
{
 unsigned int index = calls++;
 KUNIT_EXPECT_LT(active_test, index, 8U);
 if (index < 8) {
  observed_regions[index] = regions_size / sizeof(*regions);
  observed_sources[index] = src_size / sizeof(*src);
 }
 KUNIT_EXPECT_NE(active_test, dst_size, (size_t)0);
 KUNIT_EXPECT_EQ(active_test, le64_to_cpu(regions[0].mem_size), (u64)PAGE_SIZE);
 return calls == fail_at ? -EIO : 0;
}
#define qcom_scm_assign_mem_regions fake_assign
#define AVR_SECURE_BUFFER_TEST
#include "../../ports/secure-buffer/secure_buffer.c"
#undef qcom_scm_assign_mem_regions

struct fixture { struct sg_table table; struct page **pages; unsigned int count; };
static void release_fixture(void *arg)
{
 struct fixture *f=arg;
 for (unsigned int i=0;i<f->count;i++) {
  set_page_private(f->pages[i],0);
  __free_page(f->pages[i]);
 }
 sg_free_table(&f->table);
}
static struct fixture *prepare(struct kunit *test, unsigned int nents)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
 struct scatterlist *sg;
 unsigned int i;
 if (!f) return NULL;
 if (sg_alloc_table(&f->table,nents,GFP_KERNEL)) return NULL;
 f->pages=kunit_kcalloc(test,nents,sizeof(*f->pages),GFP_KERNEL);
 if (!f->pages) {sg_free_table(&f->table);return NULL;}
 if (kunit_add_action_or_reset(test,release_fixture,f)) return NULL;
 for_each_sg(f->table.sgl,sg,nents,i) {
  struct page *page=alloc_page(GFP_KERNEL);
  if (!page) return NULL;
  f->pages[f->count++]=page;
  sg_set_page(sg,page,PAGE_SIZE,0);
 }
 calls=0;fail_at=0;active_test=test;
 smp_store_release(&secure_buffer_ready,true);
 return f;
}
static int assign(struct fixture *f, unsigned int nents)
{
 u32 src=VMID_HLOS;
 int dst[2]={VMID_CP_PIXEL,VMID_CP_NON_PIXEL};
 int perms[2]={PERM_READ,PERM_READ|PERM_WRITE};
 return _hyp_assign_table(&f->table,nents,&src,1,dst,perms,2,true);
}
static void invalid_inputs(struct kunit *test)
{
 KUNIT_EXPECT_EQ(test,hyp_assign_table(NULL,NULL,0,NULL,NULL,0),-EINVAL);
 KUNIT_EXPECT_EQ(test,calls,0U);
}
static void first_batch_error(struct kunit *test)
{
 struct fixture *f=prepare(test,1);
 KUNIT_ASSERT_NOT_NULL(test,f);
 fail_at=1;
 KUNIT_EXPECT_EQ(test,assign(f,1),-EIO);
 KUNIT_EXPECT_EQ(test,calls,1U);
 KUNIT_EXPECT_EQ(test,page_private(f->pages[0]),0UL);
}
static void rollback_prefix(struct kunit *test)
{
 struct fixture *f=prepare(test,33);
 KUNIT_ASSERT_NOT_NULL(test,f);
 fail_at=2;
 KUNIT_EXPECT_EQ(test,assign(f,33),-EIO);
 KUNIT_ASSERT_EQ(test,calls,3U);
 KUNIT_EXPECT_EQ(test,observed_regions[0],32U);
 KUNIT_EXPECT_EQ(test,observed_regions[1],1U);
 KUNIT_EXPECT_EQ(test,observed_regions[2],32U);
 KUNIT_EXPECT_EQ(test,observed_sources[2],2U);
 KUNIT_EXPECT_EQ(test,page_private(f->pages[0]),0UL);
}
static void bounded_prefix(struct kunit *test)
{
 struct fixture *f=prepare(test,33);
 KUNIT_ASSERT_NOT_NULL(test,f);
 KUNIT_EXPECT_EQ(test,assign(f,3),0);
 KUNIT_ASSERT_EQ(test,calls,1U);
 KUNIT_EXPECT_EQ(test,observed_regions[0],3U);
 for (unsigned int i=0;i<33;i++)
  KUNIT_EXPECT_EQ(test,page_private(f->pages[i]),i<3 ? (unsigned long)SECURE_PAGE_MAGIC : 0UL);
}
static void full_batches(struct kunit *test)
{
 struct fixture *f=prepare(test,33);
 KUNIT_ASSERT_NOT_NULL(test,f);
 KUNIT_EXPECT_EQ(test,assign(f,33),0);
 KUNIT_ASSERT_EQ(test,calls,2U);
 KUNIT_EXPECT_EQ(test,observed_regions[0],32U);
 KUNIT_EXPECT_EQ(test,observed_regions[1],1U);
}
static struct kunit_case cases[]={
 KUNIT_CASE(invalid_inputs),KUNIT_CASE(first_batch_error),
 KUNIT_CASE(rollback_prefix),KUNIT_CASE(bounded_prefix),
 KUNIT_CASE(full_batches),{}
};
static struct kunit_suite suite={.name="avr-secure-buffer",.test_cases=cases};
kunit_test_suite(suite);
