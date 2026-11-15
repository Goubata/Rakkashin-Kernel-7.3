// SPDX-License-Identifier: GPL-2.0-only
/* Exact message prepare/unprepare methods; native runtime PM and SPI objects.
 * Hardware mode, sequencer locks, FIFO setup and DMA mapping are recorded. */
#include <kunit/test.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/spi/spi.h>
#include <linux/soc/qcom/geni-se.h>
#include <linux/qcom-geni-se-common.h>
#define SPI_LOG_ERR(...) ((void)0)
#define NUM_SPI_XFER 4
struct spi_geni_gsi { u8 unused[16]; };
struct spi_geni_master {
 struct device *dev;struct geni_se spi_rsc;void *ipc,*ipc_log_kpi;
 bool spi_kpi,shared_ee,setup,dis_autosuspend,shared_se,is_le_vm;
 int cur_xfer_mode,cur_speed_hz,cur_word_len;struct spi_geni_gsi gsi[NUM_SPI_XFER];
};
struct fixture {
 struct device *dev;struct spi_controller *ctl;struct spi_geni_master *mas;struct spi_message msg;
 int mode,map_error,fifo_error,lock_error,resume_error,unlock_usage;
 unsigned int locks,unlocks,maps,unmaps,resumes,suspends;
};
static struct fixture *backend;
static int runtime_resume(struct device *dev){struct fixture *f=dev_get_drvdata(dev);f->resumes++;return f->resume_error;}
static int runtime_suspend(struct device *dev){struct fixture *f=dev_get_drvdata(dev);f->suspends++;return 0;}
static const struct dev_pm_ops pm_ops={.runtime_resume=runtime_resume,.runtime_suspend=runtime_suspend};
static const struct device_type pm_type={.name="avr-geni-pm-ram",.pm=&pm_ops};
static int spi_geni_lock_bus(struct spi_controller *ctl){(void)ctl;backend->locks++;return backend->lock_error;}
static void spi_geni_unlock_bus(struct spi_controller *ctl){(void)ctl;backend->unlocks++;backend->unlock_usage=atomic_read(&backend->dev->power.usage_count);}
static int select_xfer_mode(struct spi_controller *ctl,struct spi_message *msg){(void)ctl;(void)msg;return backend->mode;}
static int setup_fifo_params(struct spi_device *spi,struct spi_controller *ctl){(void)spi;(void)ctl;return backend->fifo_error;}
static int spi_geni_map_buf(struct spi_geni_master *mas,struct spi_message *msg){(void)mas;(void)msg;backend->maps++;return backend->map_error;}
static void spi_geni_unmap_buf(struct spi_geni_master *mas,struct spi_message *msg){(void)mas;(void)msg;backend->unmaps++;}
static void record_select_mode(struct geni_se *se,enum geni_se_xfer_mode mode){(void)se;(void)mode;}
#define geni_se_select_mode record_select_mode
#include "generated.h"
#undef geni_se_select_mode
static int setup(struct kunit *test)
{
 struct fixture *f=kunit_kzalloc(test,sizeof(*f),GFP_KERNEL);int ret;
 if(!f)return -ENOMEM;test->priv=f;f->dev=root_device_register("avr-geni-pm-ram");if(IS_ERR(f->dev))return PTR_ERR(f->dev);
 f->dev->type=&pm_type;dev_set_drvdata(f->dev,f);f->ctl=spi_alloc_host(f->dev,sizeof(*f->mas));
 if(!f->ctl){root_device_unregister(f->dev);return -ENOMEM;}
 f->mas=spi_controller_get_devdata(f->ctl);f->mas->dev=f->dev;f->mas->spi_rsc.dev=f->dev;
 f->mas->shared_ee=f->mas->shared_se=f->mas->setup=f->mas->dis_autosuspend=true;f->mode=GENI_GPI_DMA;
 spi_message_init(&f->msg);backend=f;ret=pm_runtime_set_active(f->dev);
 if(ret){spi_controller_put(f->ctl);root_device_unregister(f->dev);return ret;}
 pm_runtime_enable(f->dev);return 0;
}
static void cleanup(struct kunit *test)
{
 struct fixture *f=test->priv;pm_runtime_barrier(f->dev);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 while(atomic_read(&f->dev->power.usage_count)>0)pm_runtime_put_noidle(f->dev);
 pm_runtime_disable(f->dev);spi_controller_put(f->ctl);backend=NULL;root_device_unregister(f->dev);
}
static void lock_failure_releases_pm_and_retry_succeeds(struct kunit *test)
{
 struct fixture *f=test->priv;f->lock_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);KUNIT_EXPECT_EQ(test,f->unlocks,0u);
 f->lock_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);
 spi_geni_unprepare_message(f->ctl,&f->msg);KUNIT_EXPECT_EQ(test,f->unlocks,1u);KUNIT_EXPECT_GT(test,f->unlock_usage,0);
}
static void map_failure_unlocks_before_releasing_power(struct kunit *test)
{
 struct fixture *f=test->priv;f->map_error=-ENOMEM;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-ENOMEM);KUNIT_EXPECT_EQ(test,f->unlocks,1u);KUNIT_EXPECT_GT(test,f->unlock_usage,0);
 KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 f->map_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);spi_geni_unprepare_message(f->ctl,&f->msg);
}
static void fifo_failure_releases_only_held_resources(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_se=false;f->mode=GENI_SE_FIFO;f->fifo_error=-EINVAL;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EINVAL);KUNIT_EXPECT_EQ(test,f->unlocks,0u);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
}
static void mode_failure_unwinds_lock_and_pm(struct kunit *test)
{
 struct fixture *f=test->priv;f->mode=-EINVAL;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EINVAL);KUNIT_EXPECT_EQ(test,f->unlocks,1u);KUNIT_EXPECT_EQ(test,f->maps,0u);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
}
static void resume_failure_is_recoverable_without_warning(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_GE(test,pm_runtime_suspend(f->dev),0);f->resume_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,f->locks,0u);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 f->resume_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);spi_geni_unprepare_message(f->ctl,&f->msg);
}
static void first_shared_message_consumes_inherited_reference(struct kunit *test)
{
 struct fixture *f=test->priv;KUNIT_ASSERT_GE(test,pm_runtime_get_sync(f->dev),0);f->mas->setup=false;f->lock_error=-EBUSY;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EBUSY);KUNIT_EXPECT_TRUE(test,f->mas->setup);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
 f->lock_error=0;KUNIT_ASSERT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),0);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);spi_geni_unprepare_message(f->ctl,&f->msg);
}
static void nonshared_hardware_reference_is_not_released(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=f->mas->shared_se=false;KUNIT_ASSERT_GE(test,pm_runtime_get_sync(f->dev),0);f->map_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),1);pm_runtime_put_sync(f->dev);
}
static void fallback_local_resume_unwinds_on_failure(struct kunit *test)
{
 struct fixture *f=test->priv;f->mas->shared_ee=f->mas->shared_se=false;KUNIT_ASSERT_GE(test,pm_runtime_suspend(f->dev),0);f->map_error=-EIO;
 KUNIT_EXPECT_EQ(test,spi_geni_prepare_message(f->ctl,&f->msg),-EIO);KUNIT_EXPECT_EQ(test,atomic_read(&f->dev->power.usage_count),0);
}
static struct kunit_case cases[]={KUNIT_CASE(lock_failure_releases_pm_and_retry_succeeds),KUNIT_CASE(map_failure_unlocks_before_releasing_power),KUNIT_CASE(fifo_failure_releases_only_held_resources),KUNIT_CASE(mode_failure_unwinds_lock_and_pm),KUNIT_CASE(resume_failure_is_recoverable_without_warning),KUNIT_CASE(first_shared_message_consumes_inherited_reference),KUNIT_CASE(nonshared_hardware_reference_is_not_released),KUNIT_CASE(fallback_local_resume_unwinds_on_failure),{}};
static struct kunit_suite suite={.name="avr-geni-pm",.init=setup,.exit=cleanup,.test_cases=cases};
kunit_test_suite(suite);
MODULE_LICENSE("GPL");MODULE_DESCRIPTION("SPI prepare error unwind with native runtime PM");
