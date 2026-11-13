// SPDX-License-Identifier: GPL-2.0-only
#include <kunit/test.h>
#include <linux/file.h>
#include <linux/mm.h>
#include <linux/highmem.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/shmem_fs.h>
#include <linux/delay.h>
#include <linux/completion.h>
#include <linux/kthread.h>

struct pool_fixture {
 struct file *file;
 struct folio *available;
 struct folio *supplied;
 unsigned int requests;
 bool registered;
 bool block;
 struct completion entered,allow,read_done,unregister_started,unregister_done;
 int read_ret,unregister_ret;
};
static struct folio *supply_folio(gfp_t gfp, int order,
                         struct shmem_inode_info *info, pgoff_t index)
{
 struct pool_fixture *f=READ_ONCE(info->folio_provider_data);
 struct folio *folio;
 if (!f || order) return NULL;
 /* Exercise the sleepable provider contract under lockdep. */
 msleep(1);
 if (f->block) {
  complete(&f->entered);
  wait_for_completion(&f->allow);
 }
 f->requests++;
 if (!f->available) return NULL;
 f->supplied=f->available;
 folio=f->available;
 f->available=NULL;
 return folio;
}
static const struct shmem_folio_provider test_provider={.alloc=supply_folio};
static void fixture_cleanup(void *arg)
{
 struct pool_fixture *f=arg;
 if (f->registered) {
  shmem_unregister_folio_provider(&test_provider);
 }
 if (!IS_ERR_OR_NULL(f->file)) {
  SHMEM_I(file_inode(f->file))->folio_provider_data=NULL;
  fput(f->file);
 }
 if (f->available) folio_put(f->available);
}
static int pool_init(struct kunit *test)
{
 struct pool_fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);
 int ret;
 if (!f) return -ENOMEM;
 test->priv=f;
 init_completion(&f->entered);init_completion(&f->allow);
 init_completion(&f->read_done);init_completion(&f->unregister_started);
 init_completion(&f->unregister_done);
 f->file=shmem_file_setup("avr-pool-test",2*PAGE_SIZE,mk_vma_flags(VMA_NORESERVE_BIT));
 if (IS_ERR(f->file)) return PTR_ERR(f->file);
 ret=kunit_add_action_or_reset(test,fixture_cleanup,f);
 if (ret) return ret;
 f->available=folio_alloc(GFP_KERNEL|__GFP_ZERO,0);
 if (!f->available) return -ENOMEM;
 SHMEM_I(file_inode(f->file))->folio_provider_data=f;
 ret=shmem_register_folio_provider(&test_provider);
 if (!ret) f->registered=true;
 return ret;
}
static void release_locked_folio(void *arg)
{
 struct folio *folio=arg;
 folio_unlock(folio);
 folio_put(folio);
}
static struct folio *read_folio(struct kunit *test, struct file *file)
{
 struct folio *folio=NULL;
 int ret=shmem_get_folio(file_inode(file),0,0,&folio,SGP_CACHE);
 if (ret) return ERR_PTR(ret);
 ret=kunit_add_action_or_reset(test,release_locked_folio,folio);
 return ret?ERR_PTR(ret):folio;
}
static void pool_folio_is_cached_and_accounted(struct kunit *test)
{
 struct pool_fixture *f=test->priv;
 struct folio *expected=f->available,*folio=read_folio(test,f->file);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,folio);
 KUNIT_EXPECT_PTR_EQ(test,folio,expected);
 KUNIT_EXPECT_PTR_EQ(test,folio_mapping(folio),f->file->f_mapping);
 KUNIT_EXPECT_TRUE(test,folio_test_swapbacked(folio));
 KUNIT_EXPECT_TRUE(test,folio_test_uptodate(folio));
 KUNIT_EXPECT_EQ(test,f->file->f_mapping->nrpages,1UL);
 KUNIT_EXPECT_EQ(test,SHMEM_I(file_inode(f->file))->alloced,1UL);
 KUNIT_EXPECT_EQ(test,f->requests,1U);
}
static void empty_pool_uses_normal_shmem(struct kunit *test)
{
 struct pool_fixture *f=test->priv;
 struct folio *folio;
 folio_put(f->available);f->available=NULL;
 folio=read_folio(test,f->file);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,folio);
 KUNIT_EXPECT_PTR_EQ(test,f->supplied,NULL);
 KUNIT_EXPECT_EQ(test,f->requests,1U);
 KUNIT_EXPECT_TRUE(test,folio_test_uptodate(folio));
}
KUNIT_DEFINE_ACTION_WRAPPER(put_file, fput, struct file *);

static void unrelated_inode_is_untouched(struct kunit *test)
{
 struct pool_fixture *f=test->priv;
 struct file *other=shmem_file_setup("avr-normal-shmem",PAGE_SIZE,EMPTY_VMA_FLAGS);
 struct folio *folio;
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,other);
 KUNIT_ASSERT_EQ(test,kunit_add_action_or_reset(test,put_file,other),0);
 KUNIT_EXPECT_PTR_EQ(test,SHMEM_I(file_inode(other))->folio_provider_data,NULL);
 folio=read_folio(test,other);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,folio);
 KUNIT_EXPECT_EQ(test,f->requests,0U);
 KUNIT_EXPECT_NOT_NULL(test,f->available);
}
static void unregister_restores_default_allocator(struct kunit *test)
{
 struct pool_fixture *f=test->priv;
 struct folio *folio;
 KUNIT_ASSERT_EQ(test,shmem_unregister_folio_provider(&test_provider),0);
 f->registered=false;
 folio=read_folio(test,f->file);
 KUNIT_ASSERT_NOT_ERR_OR_NULL(test,folio);
 KUNIT_EXPECT_PTR_NE(test,folio,f->available);
 KUNIT_EXPECT_EQ(test,f->requests,0U);
}
static void worker_wait_for_stop(void)
{
 while (!kthread_should_stop()) schedule_timeout_interruptible(1);
}
static int reader_worker(void *arg)
{
 struct pool_fixture *f=arg;
 struct folio *folio=NULL;
 f->read_ret=shmem_get_folio(file_inode(f->file),0,0,&folio,SGP_CACHE);
 if (!f->read_ret) { folio_unlock(folio);folio_put(folio); }
 complete(&f->read_done);
 worker_wait_for_stop();
 return 0;
}
static int unregister_worker(void *arg)
{
 struct pool_fixture *f=arg;
 complete(&f->unregister_started);
 f->unregister_ret=shmem_unregister_folio_provider(&test_provider);
 complete(&f->unregister_done);
 worker_wait_for_stop();
 return 0;
}
static void unregister_waits_for_sleeping_provider(struct kunit *test)
{
 struct pool_fixture *f=test->priv;
 struct task_struct *reader,*unregister=NULL;
 unsigned long entered=0,started=0,premature=0,read_done=0,unregistered=0;
 f->block=true;
 reader=kthread_run(reader_worker,f,"avr_shmem_reader");
 if (!IS_ERR(reader)) entered=wait_for_completion_timeout(&f->entered,5*HZ);
 if (entered) {
  unregister=kthread_run(unregister_worker,f,"avr_shmem_unregister");
  if (!IS_ERR(unregister)) {
   started=wait_for_completion_timeout(&f->unregister_started,5*HZ);
   premature=wait_for_completion_timeout(&f->unregister_done,msecs_to_jiffies(20));
  }
 }
 /* Release and join even if a test expectation fails. */
 complete_all(&f->allow);
 if (!IS_ERR(reader)) {
  read_done=wait_for_completion_timeout(&f->read_done,5*HZ);
  kthread_stop(reader);
 }
 if (!IS_ERR_OR_NULL(unregister)) {
  if (!premature) unregistered=wait_for_completion_timeout(&f->unregister_done,5*HZ);
  kthread_stop(unregister);
  if (!f->unregister_ret) f->registered=false;
 }
 KUNIT_EXPECT_TRUE(test,entered);
 KUNIT_EXPECT_TRUE(test,started);
 KUNIT_EXPECT_FALSE(test,premature);
 KUNIT_EXPECT_TRUE(test,read_done);
 KUNIT_EXPECT_TRUE(test,unregistered);
 KUNIT_EXPECT_EQ(test,f->read_ret,0);
 KUNIT_EXPECT_EQ(test,f->unregister_ret,0);
}
static void swap_reclaim_preserves_shmem_data(struct kunit *test)
{
 struct pool_fixture *f=test->priv;
 int reclaimed;
 KUNIT_ASSERT_EQ(test,shmem_unregister_folio_provider(&test_provider),0);
 f->registered=false;
 for (int i=0;i<2;i++) {
  struct page *page=shmem_read_mapping_page_gfp(f->file->f_mapping,i,GFP_KERNEL);
  void *address;
  KUNIT_ASSERT_NOT_ERR_OR_NULL(test,page);
  address=kmap_local_page(page);
  memset(address,0x31+i,PAGE_SIZE);
  kunmap_local(address);
  set_page_dirty_lock(page);
  put_page(page);
 }
 reclaimed=reclaim_shmem_address_space(f->file->f_mapping);
 KUNIT_EXPECT_GT(test,reclaimed,0);
 KUNIT_EXPECT_GT(test,SHMEM_I(file_inode(f->file))->swapped,0UL);
 for (int i=0;i<2;i++) {
  struct page *page=shmem_read_mapping_page_gfp(f->file->f_mapping,i,GFP_KERNEL);
  void *address;
  bool intact;
  KUNIT_ASSERT_NOT_ERR_OR_NULL(test,page);
  address=kmap_local_page(page);
  intact=!memchr_inv(address,0x31+i,PAGE_SIZE);
  kunmap_local(address);
  put_page(page);
  KUNIT_EXPECT_TRUE(test,intact);
 }
}
static struct kunit_case pool_cases[]={
 KUNIT_CASE(pool_folio_is_cached_and_accounted),
 KUNIT_CASE(empty_pool_uses_normal_shmem),
 KUNIT_CASE(unrelated_inode_is_untouched),
 KUNIT_CASE(unregister_restores_default_allocator),
 KUNIT_CASE(unregister_waits_for_sleeping_provider),
 KUNIT_CASE(swap_reclaim_preserves_shmem_data),{}
};
static struct kunit_suite pool_suite={.name="avr_shmem_pool",.init=pool_init,.test_cases=pool_cases};
kunit_test_suite(pool_suite);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Shmem pool supply, accounting, isolation and hook lifetime tests");
