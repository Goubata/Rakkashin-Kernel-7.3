// SPDX-License-Identifier: GPL-2.0-only
/* Production ring methods, native coherent DMA on private guest RAM only. */
#include <kunit/test.h>
#include <linux/platform_device.h>
#undef module_platform_driver
#define module_platform_driver(drv) static struct platform_driver * const __maybe_unused tested_driver = &(drv)
#include "../../ports/geni-bus/msm_gpi.c"
struct fixture {struct gpii gpii;struct gpi_dev dev;struct gpi_ring ring;struct device *root;struct dma_device dma;struct msm_gpi_dma_async_tx_cb_param cb,last;unsigned int calls,errors;enum msm_gpi_cb_event last_event;};
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
 if(!f)return -ENOMEM;test->priv=f;f->root=root_device_register("avr-geni-ring-ram");if(IS_ERR(f->root))return PTR_ERR(f->root);
 f->root->dma_mask=&f->root->coherent_dma_mask;ret=dma_set_mask_and_coherent(f->root,DMA_BIT_MASK(64));if(ret){root_device_unregister(f->root);return ret;}
 f->dev.dev=f->root;f->dev.klog_lvl=LOG_LVL_MASK_ALL;f->gpii.gpi_dev=&f->dev;
 INIT_LIST_HEAD(&f->dma.channels);f->dma.dev=f->root;
 f->gpii.gpii_chan[0].gpii=&f->gpii;f->gpii.gpii_chan[0].ch_ring=&f->ring;
 f->gpii.gpii_chan[0].vc.desc_free=gpi_desc_free;
 vchan_init(&f->gpii.gpii_chan[0].vc,&f->dma);return 0;
}
static void cleanup(struct kunit *test){struct fixture *f=test->priv;vchan_free_chan_resources(&f->gpii.gpii_chan[0].vc);tasklet_kill(&f->gpii.gpii_chan[0].vc.task);if(f->ring.configured)gpi_free_ring(&f->ring,&f->gpii);root_device_unregister(f->root);}
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
static void malformed_sg_is_rejected_before_mmio(struct kunit *test)
{
 struct fixture *f=test->priv;struct gpii_chan *ch=&f->gpii.gpii_chan[0];struct scatterlist sg;u8 *data=kunit_kzalloc(test,32,GFP_KERNEL);
 KUNIT_ASSERT_NOT_NULL(test,data);KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);ch->num_tre=0x55;
 /* ch_cntxt_base_reg deliberately NULL: rejected input must never read it. */
 KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,NULL,1,DMA_MEM_TO_DEV,0,NULL),NULL);
 sg_init_one(&sg,data,16);KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,&sg,0,DMA_MEM_TO_DEV,0,NULL),NULL);
 sg.length=15;KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,&sg,1,DMA_MEM_TO_DEV,0,NULL),NULL);
 sg.length=0;KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,&sg,1,DMA_MEM_TO_DEV,0,NULL),NULL);
 sg_init_table(&sg,1);KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,&sg,1,DMA_MEM_TO_DEV,0,NULL),NULL);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.wp,f->ring.base);KUNIT_EXPECT_EQ(test,ch->num_tre,0x55u);
}
static void later_bad_sg_keeps_ring_and_metadata(struct kunit *test)
{
 struct fixture *f=test->priv;struct gpii_chan *ch=&f->gpii.gpii_chan[0];struct scatterlist sg[2];u8 *data=kunit_kzalloc(test,64,GFP_KERNEL);
 KUNIT_ASSERT_NOT_NULL(test,data);KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);ch->num_tre=0x55;
 sg_init_table(sg,2);sg_set_buf(&sg[0],data,16);sg_set_buf(&sg[1],data+32,17);
 KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,sg,2,DMA_MEM_TO_DEV,0,NULL),NULL);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.wp,f->ring.base);KUNIT_EXPECT_EQ(test,ch->num_tre,0x55u);KUNIT_EXPECT_TRUE(test,list_empty(&ch->vc.desc_allocated));
}
static void overcapacity_sg_lengths_do_not_wrap(struct kunit *test)
{
 struct fixture *f=test->priv;struct gpii_chan *ch=&f->gpii.gpii_chan[0];struct scatterlist sg;u8 *data=kunit_kzalloc(test,16,GFP_KERNEL);
 KUNIT_ASSERT_NOT_NULL(test,data);KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);
 sg_init_one(&sg,data,16);sg.length=U32_MAX-15;
 KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,&sg,1,DMA_MEM_TO_DEV,0,NULL),NULL);
 KUNIT_EXPECT_PTR_EQ(test,gpi_prep_slave_sg(&ch->vc.chan,&sg,U32_MAX,DMA_MEM_TO_DEV,0,NULL),NULL);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.wp,f->ring.base);
}
static void valid_sg_prepares_native_virtual_descriptor(struct kunit *test)
{
 struct fixture *f=test->priv;struct gpii_chan *ch=&f->gpii.gpii_chan[0];struct scatterlist sg[2];struct dma_async_tx_descriptor *tx;
 u8 *data=kunit_kzalloc(test,32,GFP_KERNEL);KUNIT_ASSERT_NOT_NULL(test,data);
 KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);
 ch->ch_cntxt_base_reg=(void __iomem *)kunit_kzalloc(test,16,GFP_KERNEL);KUNIT_ASSERT_NOT_NULL(test,ch->ch_cntxt_base_reg);
 memset(data,0x5a,32);sg_init_table(sg,2);sg_set_buf(&sg[0],data,16);sg_set_buf(&sg[1],data+16,16);
 tx=gpi_prep_slave_sg(&ch->vc.chan,sg,2,DMA_MEM_TO_DEV,DMA_CTRL_ACK,NULL);KUNIT_ASSERT_NOT_NULL(test,tx);
 KUNIT_EXPECT_PTR_EQ(test,tx->chan,&ch->vc.chan);KUNIT_EXPECT_FALSE(test,list_empty(&ch->vc.desc_allocated));
 KUNIT_EXPECT_EQ(test,memcmp(f->ring.base,data,32),0);KUNIT_EXPECT_PTR_EQ(test,f->ring.wp,f->ring.base+32);KUNIT_EXPECT_EQ(test,ch->num_tre,2u);
}
static void event_client(struct dma_chan *chan,const struct msm_gpi_cb *cb,void *data)
{
 struct fixture *f=data;(void)chan;f->errors++;f->last_event=cb->cb_event;
}
static void transfer_done(void *data)
{
 struct msm_gpi_dma_async_tx_cb_param *cb=data;struct fixture *f=cb->userdata;f->last=*cb;f->calls++;
}
static int prepare_event_transfer(struct kunit *test,unsigned int elements,bool wrap)
{
 struct fixture *f=test->priv;struct gpii_chan *ch=&f->gpii.gpii_chan[0];struct scatterlist sg;struct dma_async_tx_descriptor *tx;unsigned long flags;int ret;
 u8 *data=kunit_kzalloc(test,elements*16,GFP_KERNEL);if(!data)return -ENOMEM;
 ret=gpi_alloc_ring(&f->ring,64,16,&f->gpii);if(ret)return ret;
 if(wrap)f->ring.rp=f->ring.wp=f->ring.base+f->ring.len-16;
 ch->ch_cntxt_base_reg=(void __iomem *)kunit_kzalloc(test,16,GFP_KERNEL);if(!ch->ch_cntxt_base_reg)return -ENOMEM;
 ch->client_info.callback=event_client;ch->client_info.cb_param=f;ch->pm_state=ACTIVE_STATE;f->gpii.protocol=SE_PROTOCOL_SPI;
 memset(data,0x5a,elements*16);sg_init_one(&sg,data,elements*16);
 tx=gpi_prep_slave_sg(&ch->vc.chan,&sg,1,DMA_MEM_TO_DEV,DMA_CTRL_ACK,NULL);if(!tx)return -EINVAL;
 f->cb.userdata=f;tx->callback=transfer_done;tx->callback_param=&f->cb;
 if(tx->tx_submit(tx)<0)return -EINVAL;
 spin_lock_irqsave(&ch->vc.lock,flags);vchan_issue_pending(&ch->vc);spin_unlock_irqrestore(&ch->vc.lock,flags);return 0;
}
static void valid_completion_wraps_and_notifies_once(struct kunit *test)
{
 struct fixture *f=test->priv;struct xfer_compl_event ev={.length=16,.code=MSM_GPI_TCE_EOT,.status=0x23,.type=XFER_COMPLETE_EV_TYPE};
 KUNIT_ASSERT_EQ(test,prepare_event_transfer(test,1,true),0);ev.ptr=f->ring.phys_addr+f->ring.len-16;
 gpi_process_xfer_compl_event(&f->gpii.gpii_chan[0],&ev);
 KUNIT_EXPECT_EQ(test,f->calls,1u);KUNIT_EXPECT_EQ(test,f->last.length,16u);KUNIT_EXPECT_EQ(test,f->last.status,0x23u);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.rp,f->ring.base);KUNIT_EXPECT_TRUE(test,list_empty(&f->gpii.gpii_chan[0].vc.desc_issued));
}
static void malformed_completion_preserves_pending_descriptor(struct kunit *test)
{
 struct fixture *f=test->priv;struct xfer_compl_event ev={.code=MSM_GPI_TCE_EOT};phys_addr_t invalid[3];unsigned int i;
 KUNIT_ASSERT_EQ(test,prepare_event_transfer(test,1,false),0);invalid[0]=f->ring.phys_addr-16;invalid[1]=f->ring.phys_addr+f->ring.len;invalid[2]=f->ring.phys_addr+1;
 for(i=0;i<3;i++){ev.ptr=invalid[i];gpi_process_xfer_compl_event(&f->gpii.gpii_chan[0],&ev);}
 ev.ptr=f->ring.phys_addr;ev.chid=1;gpi_process_xfer_compl_event(&f->gpii.gpii_chan[0],&ev);
 KUNIT_EXPECT_EQ(test,f->errors,4u);KUNIT_EXPECT_EQ(test,f->last_event,MSM_GPI_QUP_FW_ERROR);KUNIT_EXPECT_EQ(test,f->calls,0u);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.rp,f->ring.base);KUNIT_EXPECT_FALSE(test,list_empty(&f->gpii.gpii_chan[0].vc.desc_issued));
}
static void immediate_payload_and_index_bounds(struct kunit *test)
{
 struct fixture *f=test->priv;struct immediate_data_event ev={.code=MSM_GPI_TCE_EOT,.type=IMMEDIATE_DATA_EV_TYPE,.length=8};
 KUNIT_ASSERT_EQ(test,prepare_event_transfer(test,1,false),0);ev.tre_index=f->ring.elements;
 gpi_process_imed_data_event(&f->gpii.gpii_chan[0],&ev);ev.tre_index=0;ev.length=9;
 gpi_process_imed_data_event(&f->gpii.gpii_chan[0],&ev);KUNIT_EXPECT_EQ(test,f->errors,2u);KUNIT_EXPECT_EQ(test,f->calls,0u);
 ev.length=8;memset(ev.data_bytes,0x71,8);gpi_process_imed_data_event(&f->gpii.gpii_chan[0],&ev);
 KUNIT_EXPECT_EQ(test,f->calls,1u);KUNIT_EXPECT_EQ(test,f->last.length,8u);KUNIT_EXPECT_EQ(test,memcmp(&f->last.imed_tre,&ev,sizeof(ev)),0);
 KUNIT_EXPECT_TRUE(test,list_empty(&f->gpii.gpii_chan[0].vc.desc_issued));
}
static void q2spi_completion_keeps_high_address_bits(struct kunit *test)
{
 struct fixture *f=test->priv;struct qup_q2spi_status ev={.code=MSM_GPI_TCE_EOT,.type=QUP_TCE_TYPE_Q2SPI_STATUS,.length=16,.value=9};
 KUNIT_ASSERT_EQ(test,prepare_event_transfer(test,1,false),0);f->gpii.protocol=SE_PROTOCOL_Q2SPI;
 /* Synthetic IOVA metadata only: this is not a physical 40-bit DMA test. */
 f->ring.phys_addr=0x1200001000ULL;ev.ptr_l=(u32)f->ring.phys_addr;ev.ptr_h=f->ring.phys_addr>>32;
 gpi_process_qup_q2spi_status(&f->gpii.gpii_chan[0],&ev);
 KUNIT_EXPECT_EQ(test,f->calls,1u);KUNIT_EXPECT_EQ(test,f->errors,0u);KUNIT_EXPECT_EQ(test,(u32)f->last.q2spi_status,9u);
 KUNIT_EXPECT_PTR_EQ(test,f->ring.rp,f->ring.base+16);KUNIT_EXPECT_TRUE(test,list_empty(&f->gpii.gpii_chan[0].vc.desc_issued));
}
static void event_cursor_and_stale_scan_reject_invalid_target(struct kunit *test)
{
 struct fixture *f=test->priv;void __iomem *regs;unsigned int i;union gpi_event *events;
 KUNIT_ASSERT_EQ(test,gpi_alloc_ring(&f->ring,64,16,&f->gpii),0);regs=(void __iomem *)kunit_kzalloc(test,32,GFP_KERNEL);KUNIT_ASSERT_NOT_NULL(test,regs);
 f->gpii.ev_ring=&f->ring;f->gpii.ev_ring_rp_lsb_reg=regs;f->gpii.ieob_clr_reg=regs+8;
 for(i=0;i<2;i++){struct gpii_chan *ch=&f->gpii.gpii_chan[i];ch->gpii=&f->gpii;ch->chid=i;ch->ch_cntxt_base_reg=regs+16;ch->client_info.callback=event_client;ch->client_info.cb_param=f;}
 writel_relaxed((u32)(f->ring.phys_addr+1),regs);writel_relaxed((f->ring.phys_addr+1)>>32,regs+4);
 gpi_process_events(&f->gpii);KUNIT_EXPECT_EQ(test,f->errors,2u);KUNIT_EXPECT_PTR_EQ(test,f->ring.rp,f->ring.base);
 KUNIT_EXPECT_EQ(test,gpi_mark_stale_events(&f->gpii.gpii_chan[0]),-EINVAL);
 events=f->ring.base;events[0].xfer_compl_event.chid=0;events[0].xfer_compl_event.type=XFER_COMPLETE_EV_TYPE;
 events[1].xfer_compl_event.chid=1;events[1].xfer_compl_event.type=XFER_COMPLETE_EV_TYPE;
 writel_relaxed((u32)(f->ring.phys_addr+32),regs);writel_relaxed((f->ring.phys_addr+32)>>32,regs+4);
 KUNIT_EXPECT_EQ(test,gpi_mark_stale_events(&f->gpii.gpii_chan[0]),0);
 KUNIT_EXPECT_EQ(test,events[0].xfer_compl_event.type,(u8)STALE_EV_TYPE);KUNIT_EXPECT_EQ(test,events[1].xfer_compl_event.type,(u8)XFER_COMPLETE_EV_TYPE);
 f->gpii.gpii_chan[0].client_info.callback=NULL;f->gpii.gpii_chan[0].ch_cntxt_base_reg=NULL;
 gpi_generate_cb_event(&f->gpii.gpii_chan[0],MSM_GPI_QUP_FW_ERROR,0);KUNIT_EXPECT_EQ(test,f->errors,2u);
}
static struct kunit_case cases[]={KUNIT_CASE(invalid_geometry_and_overflow_are_rejected),KUNIT_CASE(coherent_ring_is_aligned_and_zeroed),KUNIT_CASE(nonpower_request_rounds_and_free_is_idempotent),KUNIT_CASE(full_ring_reserves_slot_and_wraps),KUNIT_CASE(recycle_and_address_translation_follow_ring),KUNIT_CASE(private_smem_arena_bounds_and_release),KUNIT_CASE(malformed_sg_is_rejected_before_mmio),KUNIT_CASE(later_bad_sg_keeps_ring_and_metadata),KUNIT_CASE(overcapacity_sg_lengths_do_not_wrap),KUNIT_CASE(valid_sg_prepares_native_virtual_descriptor),KUNIT_CASE(valid_completion_wraps_and_notifies_once),KUNIT_CASE(malformed_completion_preserves_pending_descriptor),KUNIT_CASE(immediate_payload_and_index_bounds),KUNIT_CASE(q2spi_completion_keeps_high_address_bits),KUNIT_CASE(event_cursor_and_stale_scan_reject_invalid_target),{}};
static struct kunit_suite suite={.name="avr-geni-ring",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
