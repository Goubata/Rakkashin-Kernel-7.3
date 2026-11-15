// SPDX-License-Identifier: GPL-2.0-only
/* Production ring methods, native coherent DMA on private guest RAM only. */
#include <kunit/test.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(drv) static struct platform_driver * const __maybe_unused tested_driver = &(drv)
#include "../../ports/geni-bus/msm_gpi.c"
struct fixture {struct gpii gpii;struct gpi_dev dev;struct gpi_ring ring;struct device *root;};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
 if(!f)return -ENOMEM;test->priv=f;f->root=root_device_register("avr-geni-ring-ram");if(IS_ERR(f->root))return PTR_ERR(f->root);
 f->root->dma_mask=&f->root->coherent_dma_mask;ret=dma_set_mask_and_coherent(f->root,DMA_BIT_MASK(64));if(ret){root_device_unregister(f->root);return ret;}
 f->dev.dev=f->root;f->dev.klog_lvl=LOG_LVL_MASK_ALL;f->gpii.gpi_dev=&f->dev;return 0;
}
static void cleanup(struct kunit *test){struct fixture *f=test->priv;if(f->ring.configured)gpi_free_ring(&f->ring,&f->gpii);root_device_unregister(f->root);}
static void invalid_geometry_and_overflow_are_rejected(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_EXPECT_EQ(test,gpi_alloc_ring(&f->ring,0,16,&f->gpii),-EINVAL);
 KUNIT_EXPECT_EQ(test,gpi_alloc_ring(&f->ring,1,16,&f->gpii),-EINVAL);
 KUNIT_EXPECT_EQ(test,gpi_alloc_ring(&f->ring,64,0,&f->gpii),-EINVAL);
 KUNIT_EXPECT_EQ(test,gpi_alloc_ring(&f->ring,64,3,&f->gpii),-EINVAL);
 KUNIT_EXPECT_EQ(test,gpi_alloc_ring(&f->ring,U32_MAX,16,&f->gpii),-EOVERFLOW);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.base,NULL);KUNIT_EXPECT_FALSE(test,f->ring.configured);
}
static void coherent_ring_is_aligned_and_zeroed(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int i;
 KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);
 KUNIT_EXPECT_EQ(test,f->ring.len,1024u);KUNIT_EXPECT_EQ(test,f->ring.elements,64u);KUNIT_EXPECT_EQ(test,f->ring.phys_addr%1024,0ull);
 KUNIT_EXPECT_EQ(test,gpi_ring_num_elements_avail(&f->ring),63);
 for(i=0;i<f->ring.len;i++)KUNIT_EXPECT_EQ(test,((u8*)f->ring.base)[i],(u8)0);
 KUNIT_EXPECT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),-EBUSY);
}
static void nonpower_request_rounds_and_free_is_idempotent(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,65,16,&f->gpii),0);
 KUNIT_EXPECT_EQ(test,f->ring.len,2048u);KUNIT_EXPECT_EQ(test,f->ring.elements,128u);
 KUNIT_EXPECT_EQ(test,f->ring.phys_addr%2048,0ull);
 gpi_free_ring(&f->ring,&f->gpii);gpi_free_ring(&f->ring,&f->gpii);
 KUNIT_EXPECT_FALSE(test,f->ring.configured);KUNIT_EXPECT_PTR_EQ(test,f->ring.base,NULL);
}
static void full_ring_reserves_slot_and_wraps(struct kunit *test)
{
 struct fixture *f=test->priv;void *wp=NULL;unsigned int i;
 KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);
 for(i=0;i<63;i++){KUNIT_EXPECT_EQ(test,gpi_ring_add_element(&f->ring,&wp),0);KUNIT_EXPECT_PTR_EQ(test,wp,f->ring.base+i*16);}
 KUNIT_EXPECT_EQ(test,gpi_ring_num_elements_avail(&f->ring),0);KUNIT_EXPECT_EQ(test,gpi_ring_add_element(&f->ring,&wp),-ENOMEM);
 f->ring.rp+=16;KUNIT_EXPECT_EQ(test,gpi_ring_num_elements_avail(&f->ring),1);
 KUNIT_EXPECT_EQ(test,gpi_ring_add_element(&f->ring,&wp),0);KUNIT_EXPECT_PTR_EQ(test,f->ring.wp,f->ring.base);
}
static void recycle_and_address_translation_follow_ring(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);
 f->ring.wp=f->ring.rp=f->ring.base+f->ring.len-16;gpi_ring_recycle_ev_element(&f->ring);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.wp,f->ring.base);KUNIT_EXPECT_PTR_EQ(test,f->ring.rp,f->ring.base);
 KUNIT_EXPECT_EQ(test,to_physical(&f->ring,f->ring.base+32),f->ring.phys_addr+32);
 KUNIT_EXPECT_PTR_EQ(test,to_virtual(&f->ring,f->ring.phys_addr+32),f->ring.base+32);
}
static void private_smem_arena_bounds_and_release(struct kunit *test)
{
 struct fixture *f=test->priv;void *arena=kunit_kzalloc(test,2048,GFP_KERNEL);KUNIT_ASSERT_NOT_NULL(test,arena);
 f->dev.is_use_smem_region=true;f->dev.smem_base=arena;f->dev.smem_max_addr=arena+2048;f->dev.smem_dma_iova=0x100000;
 KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);KUNIT_EXPECT_PTR_EQ(test,f->ring.base,arena);KUNIT_EXPECT_PTR_EQ(test,f->dev.smem_base,arena+1024);
 gpi_free_ring(&f->ring,&f->gpii);KUNIT_EXPECT_FALSE(test,f->ring.configured);
 f->dev.smem_max_addr=f->dev.smem_base+512;
 KUNIT_EXPECT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),-ENOMEM);KUNIT_EXPECT_PTR_EQ(test,f->dev.smem_base,arena+1024);
}
static struct kunit_case cases[]={KUNIT_CASE(invalid_geometry_and_overflow_are_rejected),KUNIT_CASE(coherent_ring_is_aligned_and_zeroed),KUNIT_CASE(nonpower_request_rounds_and_free_is_idempotent),KUNIT_CASE(full_ring_reserves_slot_and_wraps),KUNIT_CASE(recycle_and_address_translation_follow_ring),KUNIT_CASE(private_smem_arena_bounds_and_release),{}};
static struct kunit_suite suite={.name="avr-geni-ring",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
