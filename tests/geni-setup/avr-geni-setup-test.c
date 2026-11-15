// SPDX-License-Identifier: GPL-2.0-only
/* Full production master-setup method with recorded channels/MMIO, native
 * devres allocations and the native DMA slave-config callback dispatch. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/dmaengine.h>
#include <linux/spi/spi.h>
#include <linux/soc/qcom/geni-se.h>
#include <linux/qcom-geni-se-common.h>
#include <linux/msm_gpi.h>
#define SPI_LOG_DBG(...) ((void)0)
#define NUM_SPI_XFER 4
struct spi_geni_gsi { u8 unused[32]; };
struct spi_geni_master {
 struct device *dev;struct geni_se spi_rsc;void __iomem *base;void *ipc;
 bool is_le_vm,setup,master_cross_connect,gsi_mode,is_deep_sleep,shared_se,shared_ee,set_miso_sampling,dis_autosuspend;
 int oversampling,tx_fifo_depth,rx_fifo_depth,tx_fifo_width,tx_wm;
 u32 geni_init_cfg_revision;struct geni_se_ver_info ver_info;
 struct dma_chan *tx,*rx;struct spi_geni_gsi *gsi,*gsi_lock_unlock;
 struct msm_gpi_ctrl tx_event,rx_event;
};
struct fixture {
 struct device *dev;struct spi_controller *ctl;struct spi_geni_master *mas;
 struct dma_device dma;struct dma_chan chan[2];bool owned[2];
 unsigned int request_calls,alloc_calls,config_calls,version_calls,fifo_calls;
 unsigned int request_fail,alloc_fail,config_fail,live_allocs,slv_calls,master_calls;
 bool null_channel;int hw_version,proto_error;u32 if_disable;void *alloc[2];
 struct kunit *test;
};
static struct fixture *backend;
static struct dma_chan *record_request(struct device *dev,const char *name)
{
 struct fixture *f=backend;unsigned int i=!strcmp(name,"rx");(void)dev;f->request_calls++;
 if(f->request_fail==f->request_calls)return f->null_channel ? NULL : ERR_PTR(-EPROBE_DEFER);
 KUNIT_EXPECT_FALSE(f->test,f->owned[i]);f->owned[i]=true;return &f->chan[i];
}
static void record_release(struct dma_chan *chan)
{
 struct fixture *f=backend;unsigned int i=chan==&f->chan[1];
 KUNIT_EXPECT_PTR_EQ(f->test,chan,&f->chan[i]);KUNIT_EXPECT_TRUE(f->test,f->owned[i]);
 f->owned[i]=false;chan->private=NULL;
}
static int record_config(struct dma_chan *chan,struct dma_slave_config *config)
{
 struct fixture *f=backend;(void)chan;KUNIT_EXPECT_PTR_EQ(f->test,config,NULL);f->config_calls++;
 /* Neither setup nor resource pointers may be published during config. */
 KUNIT_EXPECT_FALSE(f->test,f->mas->setup);KUNIT_EXPECT_PTR_EQ(f->test,f->mas->tx,NULL);
 return f->config_fail==f->config_calls ? -EIO : 0;
}
static void *record_alloc(struct device *dev,size_t size,gfp_t gfp)
{
 struct fixture *f=backend;void *p;unsigned int i;f->alloc_calls++;
 if(f->alloc_fail==f->alloc_calls)return NULL;
 p=devm_kzalloc(dev,size,gfp);if(!p)return NULL;
 for(i=0;i<ARRAY_SIZE(f->alloc);i++)if(!f->alloc[i]){f->alloc[i]=p;f->live_allocs++;return p;}
 KUNIT_FAIL(f->test,"allocation tracker exhausted");devm_kfree(dev,p);return NULL;
}
static void record_free(struct device *dev,void *p)
{
 struct fixture *f=backend;unsigned int i;
 for(i=0;i<ARRAY_SIZE(f->alloc);i++)if(f->alloc[i]==p){f->alloc[i]=NULL;f->live_allocs--;devm_kfree(dev,p);return;}
 KUNIT_FAIL(f->test,"release without allocation ownership");
}
static int spi_verify_proto(struct spi_geni_master *mas){(void)mas;return backend->proto_error;}
static void spi_slv_setup(struct spi_geni_master *mas){(void)mas;backend->slv_calls++;}
static void spi_master_setup(struct spi_geni_master *mas){(void)mas;backend->master_calls++;}
static void spi_gsi_ch_cb(struct dma_chan *chan,const struct msm_gpi_cb *cb,void *ptr){(void)chan;(void)cb;(void)ptr;}
static u32 record_read(void __iomem *base,u32 offset){(void)base;return offset==GENI_IF_DISABLE_RO ? backend->if_disable : 7;}
static int record_version(struct geni_se *se){(void)se;backend->version_calls++;return backend->hw_version;}
static u32 record_depth(struct geni_se *se){(void)se;return 16;}
static void record_init(struct geni_se *se,u32 rx,u32 tx){(void)se;(void)rx;KUNIT_EXPECT_EQ(backend->test,tx,14u);backend->fifo_calls++;}
static void *record_ipc(int pages,const char *name,u32 feature){(void)pages;(void)name;(void)feature;return backend;}
static void spi_geni_set_sampling_rate(struct spi_geni_master *mas,unsigned int major,unsigned int minor){(void)mas;(void)major;(void)minor;}
static int record_fw(void __iomem *base){(void)base;return 1;}
#define dma_request_slave_channel record_request
#define dma_release_channel record_release
#define devm_kzalloc record_alloc
#define devm_kfree record_free
#define geni_read_reg record_read
#define geni_se_get_qup_hw_version record_version
#define geni_se_get_tx_fifo_depth record_depth
#define geni_se_get_rx_fifo_depth record_depth
#define geni_se_get_tx_fifo_width record_depth
#define geni_se_init record_init
#define ipc_log_context_create record_ipc
#define geni_se_common_get_m_fw record_fw
#define geni_se_common_get_s_fw record_fw
#include "generated.h"
#undef devm_kzalloc
#undef devm_kfree
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);unsigned int i;
 if(!f)return -ENOMEM;test->priv=f;f->test=test;
 f->dev=root_device_register("avr-geni-setup-ram");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
 f->ctl=spi_alloc_host(f->dev,sizeof(*f->mas));if(!f->ctl){root_device_unregister(f->dev);return -ENOMEM;}
 f->mas=spi_controller_get_devdata(f->ctl);f->mas->dev=f->dev;f->mas->shared_se=true;
 f->if_disable=FIFO_IF_DISABLE;f->hw_version=0x20050000;f->dma.device_config=record_config;
 for(i=0;i<2;i++)f->chan[i].device=&f->dma;backend=f;return 0;
}
static void release_success(struct fixture *f)
{
 if(f->mas->tx){record_release(f->mas->tx);f->mas->tx=NULL;}
 if(f->mas->rx){record_release(f->mas->rx);f->mas->rx=NULL;}
 if(f->mas->gsi){record_free(f->dev,f->mas->gsi);f->mas->gsi=NULL;}
 if(f->mas->gsi_lock_unlock){record_free(f->dev,f->mas->gsi_lock_unlock);f->mas->gsi_lock_unlock=NULL;}
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;release_success(f);
 KUNIT_EXPECT_FALSE(test,f->owned[0]);KUNIT_EXPECT_FALSE(test,f->owned[1]);KUNIT_EXPECT_EQ(test,f->live_allocs,0u);
 spi_controller_put(f->ctl);root_device_unregister(f->dev);backend=NULL;
}
static void expect_unpublished(struct kunit *test)
{
 struct fixture *f=test->priv;
 KUNIT_EXPECT_FALSE(test,f->mas->setup);KUNIT_EXPECT_PTR_EQ(test,f->mas->tx,NULL);KUNIT_EXPECT_PTR_EQ(test,f->mas->rx,NULL);
 KUNIT_EXPECT_PTR_EQ(test,f->mas->gsi,NULL);KUNIT_EXPECT_PTR_EQ(test,f->mas->gsi_lock_unlock,NULL);
 KUNIT_EXPECT_FALSE(test,f->owned[0]);KUNIT_EXPECT_FALSE(test,f->owned[1]);KUNIT_EXPECT_EQ(test,f->live_allocs,0u);
}
static void clear_fault_and_retry(struct kunit *test)
{
 struct fixture *f=test->priv;expect_unpublished(test);
 f->request_fail=f->alloc_fail=f->config_fail=0;f->hw_version=0x20050000;
 KUNIT_ASSERT_EQ(test,spi_geni_mas_setup(f->ctl),0);KUNIT_EXPECT_TRUE(test,f->mas->setup);
 KUNIT_EXPECT_PTR_EQ(test,f->mas->tx,&f->chan[0]);KUNIT_EXPECT_PTR_EQ(test,f->mas->rx,&f->chan[1]);KUNIT_EXPECT_EQ(test,f->live_allocs,2u);
}
static void request_failures_are_atomic_and_retryable(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int i;
 for(i=1;i<=2;i++){
  f->request_fail=f->request_calls+i;KUNIT_EXPECT_EQ(test,spi_geni_mas_setup(f->ctl),-EPROBE_DEFER);expect_unpublished(test);
 }
 clear_fault_and_retry(test);
}
static void null_channel_returns_enodev_without_false_success(struct kunit *test)
{
 struct fixture *f=test->priv;f->null_channel=true;f->request_fail=2;
 KUNIT_EXPECT_EQ(test,spi_geni_mas_setup(f->ctl),-ENODEV);clear_fault_and_retry(test);
}
static void allocation_failures_release_devres_and_channels(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int i;
 for(i=1;i<=2;i++){
  f->alloc_fail=f->alloc_calls+i;KUNIT_EXPECT_EQ(test,spi_geni_mas_setup(f->ctl),-ENOMEM);expect_unpublished(test);
 }
 clear_fault_and_retry(test);
}
static void config_failures_release_all_new_resources(struct kunit *test)
{
 struct fixture *f=test->priv;unsigned int i;
 for(i=1;i<=2;i++){
  f->config_fail=f->config_calls+i;KUNIT_EXPECT_EQ(test,spi_geni_mas_setup(f->ctl),-EIO);expect_unpublished(test);
 }
 clear_fault_and_retry(test);
}
static void zero_version_rolls_back_before_setup_publication(struct kunit *test)
{
 struct fixture *f=test->priv;f->hw_version=0;
 KUNIT_EXPECT_EQ(test,spi_geni_mas_setup(f->ctl),-ENXIO);clear_fault_and_retry(test);
}
static void shared_first_setup_preserves_reference_handoff_flag(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=true;
 KUNIT_ASSERT_EQ(test,spi_geni_mas_setup(f->ctl),0);KUNIT_EXPECT_FALSE(test,f->mas->setup);
 KUNIT_EXPECT_PTR_EQ(test,f->mas->tx,&f->chan[0]);KUNIT_EXPECT_EQ(test,f->live_allocs,2u);
}
static void nonshared_engine_needs_no_lock_allocation(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_se=false;
 KUNIT_ASSERT_EQ(test,spi_geni_mas_setup(f->ctl),0);KUNIT_EXPECT_TRUE(test,f->mas->setup);
 KUNIT_EXPECT_EQ(test,f->live_allocs,1u);KUNIT_EXPECT_PTR_EQ(test,f->mas->gsi_lock_unlock,NULL);
}
static void fifo_path_uses_no_dma_resources(struct kunit *test)
{
 struct fixture *f=test->priv;f->if_disable=0;
 KUNIT_ASSERT_EQ(test,spi_geni_mas_setup(f->ctl),0);KUNIT_EXPECT_TRUE(test,f->mas->setup);
 KUNIT_EXPECT_EQ(test,f->request_calls,0u);KUNIT_EXPECT_EQ(test,f->fifo_calls,1u);KUNIT_EXPECT_EQ(test,f->live_allocs,0u);
}
static void le_validation_failure_and_success_skip_common_mmio(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->is_le_vm=true;f->proto_error=-ENXIO;
 KUNIT_EXPECT_EQ(test,spi_geni_mas_setup(f->ctl),-ENXIO);expect_unpublished(test);KUNIT_EXPECT_EQ(test,f->request_calls,0u);
 f->proto_error=0;KUNIT_ASSERT_EQ(test,spi_geni_mas_setup(f->ctl),0);KUNIT_EXPECT_EQ(test,f->version_calls,0u);
 KUNIT_EXPECT_TRUE(test,f->mas->setup);KUNIT_EXPECT_EQ(test,f->live_allocs,2u);
}
static struct kunit_case cases[]={KUNIT_CASE(request_failures_are_atomic_and_retryable),KUNIT_CASE(null_channel_returns_enodev_without_false_success),KUNIT_CASE(allocation_failures_release_devres_and_channels),KUNIT_CASE(config_failures_release_all_new_resources),KUNIT_CASE(zero_version_rolls_back_before_setup_publication),KUNIT_CASE(shared_first_setup_preserves_reference_handoff_flag),KUNIT_CASE(nonshared_engine_needs_no_lock_allocation),KUNIT_CASE(fifo_path_uses_no_dma_resources),KUNIT_CASE(le_validation_failure_and_success_skip_common_mmio),{}};
static struct kunit_suite suite={.name="avr-geni-setup",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("GENI SPI initialization fault ownership with native devres");
