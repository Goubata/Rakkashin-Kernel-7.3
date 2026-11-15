// SPDX-License-Identifier: GPL-2.0-only
/* Exact SPI map/unmap methods; native DMA on private RAM with selected faults.
 * Zero-IOVA case explicitly uses recorded mappings, never physical address 0. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/dma-mapping.h>
#include <linux/spi/spi.h>
#include <linux/soc/qcom/geni-se.h>
#include <linux/qcom-geni-se-common.h>
struct spi_geni_master { struct device *wrapper_dev; };
struct lease { dma_addr_t addr;size_t size;enum dma_data_direction dir;bool active; };
struct fixture {
 struct device *dev;struct spi_geni_master mas;struct spi_message msg;
 struct spi_transfer transfers[3];struct lease leases[16];unsigned int calls,fail,unmaps,live,bad;
 bool recorded;
};
static struct fixture *backend;
static int map_buffer(struct device *dev,dma_addr_t *address,void *buffer,size_t size,enum dma_data_direction dir)
{
 struct fixture *f=backend;unsigned int index=f->calls++;int ret;
 if(index+1==f->fail){*address=DMA_MAPPING_ERROR;return -EIO;}
 if(index>=ARRAY_SIZE(f->leases))return -E2BIG;
 if(f->recorded){*address=index*32;ret=0;}
 else ret=geni_se_common_iommu_map_buf(dev,address,buffer,size,dir);
 if(ret)return ret;
 f->leases[index]=(struct lease){.addr=*address,.size=size,.dir=dir,.active=true};f->live++;return 0;
}
static int unmap_buffer(struct device *dev,dma_addr_t *address,size_t size,enum dma_data_direction dir)
{
 struct fixture *f=backend;unsigned int i;
 for(i=0;i<ARRAY_SIZE(f->leases);i++)if(f->leases[i].active && f->leases[i].addr==*address && f->leases[i].dir==dir && f->leases[i].size==size){
  if(!f->recorded)geni_se_common_iommu_unmap_buf(dev,address,size,dir);
  f->leases[i].active=false;f->unmaps++;f->live--;return 0;
 }
 f->bad++;return -EINVAL;
}
#define geni_se_common_iommu_map_buf map_buffer
#define geni_se_common_iommu_unmap_buf unmap_buffer
#include "generated.h"
#undef geni_se_common_iommu_map_buf
#undef geni_se_common_iommu_unmap_buf
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);unsigned int i;int ret;
 if(!f)return -ENOMEM;test->priv=f;f->dev=root_device_register("avr-geni-map-ram");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
 f->dev->dma_mask=&f->dev->coherent_dma_mask;ret=dma_set_mask_and_coherent(f->dev,DMA_BIT_MASK(64));if(ret){root_device_unregister(f->dev);return ret;}
 f->mas.wrapper_dev=f->dev;spi_message_init(&f->msg);backend=f;
 for(i=0;i<3;i++){
  f->transfers[i].len=32;f->transfers[i].tx_buf=kunit_kzalloc(test,32,GFP_KERNEL);f->transfers[i].rx_buf=kunit_kzalloc(test,32,GFP_KERNEL);
  if(!f->transfers[i].tx_buf || !f->transfers[i].rx_buf){root_device_unregister(f->dev);return -ENOMEM;}
  spi_message_add_tail(&f->transfers[i],&f->msg);
 }
 return 0;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int i;
 for(i=0;i<ARRAY_SIZE(f->leases);i++)if(f->leases[i].active && !f->recorded)
  dma_unmap_single(f->dev,f->leases[i].addr,f->leases[i].size,f->leases[i].dir);
 KUNIT_EXPECT_EQ(test,f->live,0u);KUNIT_EXPECT_EQ(test,f->bad,0u);backend=NULL;root_device_unregister(f->dev);
}
static void successful_message_maps_and_releases_once(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_ASSERT_EQ(test,spi_geni_map_buf(&f->mas,&f->msg),0);KUNIT_EXPECT_EQ(test,f->live,6u);
 spi_geni_unmap_buf(&f->mas,&f->msg);KUNIT_EXPECT_EQ(test,f->live,0u);KUNIT_EXPECT_EQ(test,f->unmaps,6u);
 spi_geni_unmap_buf(&f->mas,&f->msg);KUNIT_EXPECT_EQ(test,f->unmaps,6u);
}
static void every_failure_position_unwinds_prior_maps(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int position,i;
 for(position=1;position<=6;position++){
  f->calls=f->unmaps=0;f->fail=position;
  KUNIT_EXPECT_EQ(test,spi_geni_map_buf(&f->mas,&f->msg),-EIO);
  KUNIT_EXPECT_EQ(test,f->live,0u);KUNIT_EXPECT_EQ(test,f->unmaps,position-1);
  for(i=0;i<3;i++){KUNIT_EXPECT_EQ(test,f->transfers[i].rx_dma,(dma_addr_t)DMA_MAPPING_ERROR);KUNIT_EXPECT_EQ(test,f->transfers[i].tx_dma,(dma_addr_t)DMA_MAPPING_ERROR);}
  spi_geni_unmap_buf(&f->mas,&f->msg);KUNIT_EXPECT_EQ(test,f->unmaps,position-1);
 }
}
static void retry_after_failure_can_map_full_message(struct kunit *test)
{
 struct fixture *f=test->priv;f->fail=4;KUNIT_EXPECT_EQ(test,spi_geni_map_buf(&f->mas,&f->msg),-EIO);
 f->calls=0;f->fail=0;KUNIT_ASSERT_EQ(test,spi_geni_map_buf(&f->mas,&f->msg),0);
 KUNIT_EXPECT_EQ(test,f->live,6u);spi_geni_unmap_buf(&f->mas,&f->msg);KUNIT_EXPECT_EQ(test,f->live,0u);
}
static void zero_length_and_unidirectional_transfers(struct kunit *test)
{
 struct fixture *f=test->priv;f->transfers[0].len=0;f->transfers[1].tx_buf=NULL;f->transfers[2].rx_buf=NULL;
 KUNIT_ASSERT_EQ(test,spi_geni_map_buf(&f->mas,&f->msg),0);KUNIT_EXPECT_EQ(test,f->calls,2u);
 spi_geni_unmap_buf(&f->mas,&f->msg);KUNIT_EXPECT_EQ(test,f->unmaps,2u);
}
static void zero_iova_is_owned_in_recorded_backend(struct kunit *test)
{
 struct fixture *f=test->priv;f->recorded=true;
 KUNIT_ASSERT_EQ(test,spi_geni_map_buf(&f->mas,&f->msg),0);KUNIT_EXPECT_EQ(test,f->transfers[0].rx_dma,(dma_addr_t)0);
 spi_geni_unmap_buf(&f->mas,&f->msg);KUNIT_EXPECT_EQ(test,f->unmaps,6u);KUNIT_EXPECT_EQ(test,f->live,0u);
}
static struct kunit_case cases[]={KUNIT_CASE(successful_message_maps_and_releases_once),KUNIT_CASE(every_failure_position_unwinds_prior_maps),KUNIT_CASE(retry_after_failure_can_map_full_message),KUNIT_CASE(zero_length_and_unidirectional_transfers),KUNIT_CASE(zero_iova_is_owned_in_recorded_backend),{}};
static struct kunit_suite suite={.name="avr-geni-map",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("SPI mapping rollback on native DMA with selected faults");
